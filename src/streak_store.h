/**
 * streak_store.h — WAL-safe NVS persistence for streak/level data
 * 
 * Write-ahead log pattern:
 *   1. Write new data to "wal" namespace
 *   2. Commit: copy "wal" → "main" namespace  
 *   3. Clear "wal" namespace
 * 
 * On boot: if "wal" exists, it means a previous write was interrupted
 *   → recover by committing the WAL data to main
 * 
 * This ensures data integrity even if power is lost mid-write.
 */

#ifndef STREAK_STORE_H
#define STREAK_STORE_H

#include <Preferences.h>
#include <Arduino.h>

// ── Level definitions (must match web UI) ──
struct Level {
    const char* name;
    int threshold;
    const char* emoji;  // for serial debug only; LVGL uses text
};

static const Level LEVELS[] = {
    {"Starter",      0,   "🌱"},
    {"Consistent",   5,   "🔥"},
    {"Dedicated",   15,   "⚡"},
    {"Unstoppable", 30,   "💎"},
    {"Legend",       50,   "👑"},
    {"Titan",       100,  "🏆"},
};
static const int NUM_LEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);

// ── Streak data ──
struct StreakData {
    int streak;
    int lastYear;   // e.g. 2026
    int lastMonth;  // 1-12
    int lastDay;    // 1-31
};

// ── NVS namespaces ──
#define NS_MAIN "streak"
#define NS_WAL  "streak_wal"

class StreakStore {
public:
    StreakData data;

    void begin() {
        // Check for interrupted write (WAL recovery)
        recoverWAL();
        // Load from main namespace
        loadMain();
        Serial.printf("[Streak] Loaded: streak=%d, last=%04d-%02d-%02d\n",
                      data.streak, data.lastYear, data.lastMonth, data.lastDay);
    }

    // Call when all tasks are completed for a given day offset
    void markDayComplete(int dayOffset = 0) {
        struct tm now;
        if (!getLocalTime(&now)) return;

        now.tm_mday += dayOffset;
        mktime(&now);  // normalize

        int y = now.tm_year + 1900;
        int m = now.tm_mon + 1;
        int d = now.tm_mday;

        // Already marked this date?
        if (data.lastYear == y && data.lastMonth == m && data.lastDay == d) return;

        // Check if this date is exactly 1 day after last completed
        bool isConsecutive = isYesterday(data.lastYear, data.lastMonth, data.lastDay, y, m, d);

        if (isConsecutive) {
            data.streak++;
        } else {
            data.streak = 1; // streak broken
        }

        data.lastYear = y;
        data.lastMonth = m;
        data.lastDay = d;

        // WAL-safe write
        saveWithWAL();

        Serial.printf("[Streak] Day complete! streak=%d\n", data.streak);
    }

    const Level& getLevel() const {
        int idx = 0;
        for (int i = 0; i < NUM_LEVELS; i++) {
            if (data.streak >= LEVELS[i].threshold) idx = i;
        }
        return LEVELS[idx];
    }

    const Level* getNextLevel() const {
        for (int i = 0; i < NUM_LEVELS; i++) {
            if (data.streak < LEVELS[i].threshold) return &LEVELS[i];
        }
        return nullptr;
    }

    int getProgressToNext() const {
        const Level& cur = getLevel();
        const Level* next = getNextLevel();
        if (!next) return 100;
        int range = next->threshold - cur.threshold;
        int progress = data.streak - cur.threshold;
        return (progress * 100) / range;
    }

private:
    void loadMain() {
        Preferences prefs;
        prefs.begin(NS_MAIN, true); // read-only
        data.streak   = prefs.getInt("streak", 0);
        data.lastYear  = prefs.getInt("lastY", 0);
        data.lastMonth = prefs.getInt("lastM", 0);
        data.lastDay   = prefs.getInt("lastD", 0);
        prefs.end();
    }

    void saveWithWAL() {
        // Step 1: Write to WAL namespace
        {
            Preferences wal;
            wal.begin(NS_WAL, false);
            wal.putInt("streak", data.streak);
            wal.putInt("lastY",  data.lastYear);
            wal.putInt("lastM",  data.lastMonth);
            wal.putInt("lastD",  data.lastDay);
            wal.putBool("valid", true);  // mark WAL as complete
            wal.end();
        }

        // Step 2: Commit WAL → main
        {
            Preferences main;
            main.begin(NS_MAIN, false);
            main.putInt("streak", data.streak);
            main.putInt("lastY",  data.lastYear);
            main.putInt("lastM",  data.lastMonth);
            main.putInt("lastD",  data.lastDay);
            main.end();
        }

        // Step 3: Clear WAL (transaction complete)
        {
            Preferences wal;
            wal.begin(NS_WAL, false);
            wal.clear();
            wal.end();
        }

        Serial.println("[Streak] Saved to NVS (WAL-safe)");
    }

    void recoverWAL() {
        Preferences wal;
        wal.begin(NS_WAL, true); // read-only
        bool valid = wal.getBool("valid", false);

        if (valid) {
            // WAL has uncommitted data — power was lost after step 1
            Serial.println("[Streak] WAL recovery: found uncommitted data, committing...");
            int streak = wal.getInt("streak", 0);
            int lastY  = wal.getInt("lastY", 0);
            int lastM  = wal.getInt("lastM", 0);
            int lastD  = wal.getInt("lastD", 0);
            wal.end();

            // Commit to main
            Preferences main;
            main.begin(NS_MAIN, false);
            main.putInt("streak", streak);
            main.putInt("lastY",  lastY);
            main.putInt("lastM",  lastM);
            main.putInt("lastD",  lastD);
            main.end();

            // Clear WAL
            Preferences walClear;
            walClear.begin(NS_WAL, false);
            walClear.clear();
            walClear.end();

            Serial.println("[Streak] WAL recovery complete");
        } else {
            wal.end();
        }
    }

    // Simple "is yesterday" check
    bool isYesterday(int y1, int m1, int d1, int y2, int m2, int d2) {
        // Convert both to tm, use mktime to get epoch, compare
        struct tm t1 = {}, t2 = {};
        t1.tm_year = y1 - 1900; t1.tm_mon = m1 - 1; t1.tm_mday = d1;
        t2.tm_year = y2 - 1900; t2.tm_mon = m2 - 1; t2.tm_mday = d2;
        time_t epoch1 = mktime(&t1);
        time_t epoch2 = mktime(&t2);
        if (epoch1 == -1 || epoch2 == -1) return false;
        long diff = (epoch2 - epoch1) / 86400;
        return diff == 1;
    }
};

#endif /* STREAK_STORE_H */
