/**
 * Calendar fetcher — Google Calendar API + iCal URL support.
 * Reads calendar list from calendar_store.h.
 */

#ifndef CALENDAR_FETCH_H
#define CALENDAR_FETCH_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFi.h>
#include "calendar_store.h"
#include "credentials.h"

#define MAX_TASKS      10
#define MAX_TITLE_LEN  64

struct CalTask {
    char id[64];
    char title[MAX_TITLE_LEN];
    char time[8];   // "HH:MM" or "All day"
    bool completed;
};

static CalTask cal_tasks[MAX_TASKS];
static int     cal_task_count      = 0;
static int     cal_day_offset      = 0;
static int     cal_loaded_day_offset = 0;

// ══════════════════════════════════════
// COMPLETION CACHE
// ══════════════════════════════════════

#define MAX_DAY_COMPLETION_CACHE    3
#define MAX_PERSIST_KEYS_PER_DAY    MAX_TASKS
#define MAX_PERSIST_KEY_LEN         48

struct DayCompletionCache {
    char date[11];
    char keys[MAX_PERSIST_KEYS_PER_DAY][MAX_PERSIST_KEY_LEN];
    int  key_count;
    bool used;
};

static DayCompletionCache completion_cache[MAX_DAY_COMPLETION_CACHE];
static int completion_cache_next_slot = 0;

static void getDateKeyForOffset(int offset, char* out, size_t len) {
    struct tm now;
    if (!getLocalTime(&now)) { snprintf(out, len, "1970-01-01"); return; }
    now.tm_mday += offset;
    mktime(&now);
    snprintf(out, len, "%04d-%02d-%02d", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
}

static DayCompletionCache* getDayCache(const char* dateKey, bool create_if_missing) {
    int free_idx = -1;
    for (int i = 0; i < MAX_DAY_COMPLETION_CACHE; i++) {
        if (completion_cache[i].used && strcmp(completion_cache[i].date, dateKey) == 0)
            return &completion_cache[i];
        if (!completion_cache[i].used && free_idx == -1) free_idx = i;
    }
    if (!create_if_missing) return nullptr;
    int idx = (free_idx != -1) ? free_idx : completion_cache_next_slot;
    if (free_idx == -1)
        completion_cache_next_slot = (completion_cache_next_slot + 1) % MAX_DAY_COMPLETION_CACHE;
    completion_cache[idx].used = true;
    completion_cache[idx].key_count = 0;
    strncpy(completion_cache[idx].date, dateKey, sizeof(completion_cache[idx].date) - 1);
    completion_cache[idx].date[sizeof(completion_cache[idx].date) - 1] = '\0';
    return &completion_cache[idx];
}

static void addCacheKey(DayCompletionCache* cache, const char* key) {
    if (!cache || !key || key[0] == '\0') return;
    for (int i = 0; i < cache->key_count; i++)
        if (strcmp(cache->keys[i], key) == 0) return;
    if (cache->key_count >= MAX_PERSIST_KEYS_PER_DAY) return;
    strncpy(cache->keys[cache->key_count], key, MAX_PERSIST_KEY_LEN - 1);
    cache->keys[cache->key_count][MAX_PERSIST_KEY_LEN - 1] = '\0';
    cache->key_count++;
}

static void makeTaskCompositeKey(const CalTask& task, char* out, size_t len) {
    snprintf(out, len, "%s__%s", task.title, task.time);
}

static void saveCurrentDayCompletionState() {
    char dateKey[11];
    getDateKeyForOffset(cal_loaded_day_offset, dateKey, sizeof(dateKey));
    DayCompletionCache* cache = getDayCache(dateKey, true);
    if (!cache) return;
    cache->key_count = 0;
    for (int i = 0; i < cal_task_count; i++) {
        if (!cal_tasks[i].completed) continue;
        addCacheKey(cache, cal_tasks[i].id);
        char composite[MAX_PERSIST_KEY_LEN];
        makeTaskCompositeKey(cal_tasks[i], composite, sizeof(composite));
        addCacheKey(cache, composite);
    }
}

static bool isTaskCompletedForOffset(const CalTask& task, int offset) {
    char dateKey[11];
    getDateKeyForOffset(offset, dateKey, sizeof(dateKey));
    DayCompletionCache* cache = getDayCache(dateKey, false);
    if (!cache) return false;
    char composite[MAX_PERSIST_KEY_LEN];
    makeTaskCompositeKey(task, composite, sizeof(composite));
    for (int i = 0; i < cache->key_count; i++)
        if (strcmp(cache->keys[i], task.id) == 0 || strcmp(cache->keys[i], composite) == 0)
            return true;
    return false;
}

// ══════════════════════════════════════
// HELPERS
// ══════════════════════════════════════

static void parseTime(const char* iso, char* out, size_t len) {
    const char* t = strchr(iso, 'T');
    if (t && strlen(t) >= 6) snprintf(out, len, "%.5s", t + 1);
    else snprintf(out, len, "All day");
}

static void getOffsetDateLabel(char* out, size_t len) {
    if (cal_day_offset == 0)       snprintf(out, len, "Today");
    else if (cal_day_offset == 1)  snprintf(out, len, "Tomorrow");
    else if (cal_day_offset == -1) snprintf(out, len, "Yesterday");
    else {
        struct tm now; getLocalTime(&now);
        now.tm_mday += cal_day_offset; mktime(&now);
        strftime(out, len, "%b %d", &now);
    }
}

// URL-encodes just the characters needed for calendar IDs.
static String calUrlEncode(const char* s) {
    String out = "";
    for (; *s; s++) {
        if (*s == '@') out += "%40";
        else out += *s;
    }
    return out;
}

// Extracts the value of an iCal property from a VEVENT block.
// Handles both "PROP:value" and "PROP;params:value" forms.
// The block should start with "\n" so property names match at line starts.
static String icalProp(const String& block, const char* prop) {
    String pColon = String("\n") + prop + ":";
    String pSemi  = String("\n") + prop + ";";
    int i1 = block.indexOf(pColon);
    int i2 = block.indexOf(pSemi);
    int idx = -1;
    if (i1 >= 0 && i2 >= 0) idx = min(i1, i2);
    else if (i1 >= 0) idx = i1;
    else if (i2 >= 0) idx = i2;
    if (idx < 0) return "";

    int colon = block.indexOf(":", idx + 1);
    if (colon < 0) return "";
    int end = block.indexOf("\n", colon + 1);
    if (end < 0) end = block.length();
    String val = block.substring(colon + 1, end);
    val.trim();
    return val;
}

// ══════════════════════════════════════
// GOOGLE CALENDAR FETCH
// ══════════════════════════════════════

static bool fetchGCalForDay(const CalendarEntry& entry) {
    struct tm now;
    if (!getLocalTime(&now)) return false;
    now.tm_mday += cal_day_offset;
    mktime(&now);

    char timeMin[32], timeMax[32];
    snprintf(timeMin, sizeof(timeMin), "%04d-%02d-%02dT00:00:00Z",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    snprintf(timeMax, sizeof(timeMax), "%04d-%02d-%02dT23:59:59Z",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    String calId = calUrlEncode(entry.url);
    char url[512];
    snprintf(url, sizeof(url),
        "https://www.googleapis.com/calendar/v3/calendars/%s/events"
        "?key=%s&timeMin=%s&timeMax=%s&singleEvents=true&orderBy=startTime",
        calId.c_str(), GCAL_API_KEY, timeMin, timeMax);

    Serial.printf("[GCal] Fetching: %s\n", entry.name);

    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[GCal] HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, payload)) {
        Serial.println("[GCal] JSON parse error");
        return false;
    }

    JsonArray items = doc["items"];
    for (JsonObject item : items) {
        if (cal_task_count >= MAX_TASKS) break;
        CalTask &t = cal_tasks[cal_task_count];

        const char* id = item["id"] | "unknown";
        strncpy(t.id, id, sizeof(t.id) - 1);
        t.id[sizeof(t.id) - 1] = '\0';

        const char* summary = item["summary"] | "Untitled";
        strncpy(t.title, summary, MAX_TITLE_LEN - 1);
        t.title[MAX_TITLE_LEN - 1] = '\0';

        JsonObject startObj = item["start"];
        if (startObj.containsKey("dateTime")) {
            const char* dt = startObj["dateTime"];
            if (dt) parseTime(dt, t.time, sizeof(t.time));
            else    snprintf(t.time, sizeof(t.time), "All day");
        } else {
            snprintf(t.time, sizeof(t.time), "All day");
        }

        t.completed = isTaskCompletedForOffset(t, cal_day_offset);
        cal_task_count++;
    }
    return true;
}

// ══════════════════════════════════════
// ICAL FETCH + PARSE
// ══════════════════════════════════════

static bool fetchICalForDay(const CalendarEntry& entry) {
    HTTPClient http;
    http.begin(entry.url);
    http.setTimeout(10000);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[iCal] HTTP %d for %s\n", code, entry.name);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    // Build today's YYYYMMDD string for date comparison
    struct tm now;
    if (!getLocalTime(&now)) return false;
    now.tm_mday += cal_day_offset;
    mktime(&now);
    char todayStr[9];
    snprintf(todayStr, sizeof(todayStr), "%04d%02d%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    // Normalise line endings to \n
    body.replace("\r\n", "\n");
    body.replace("\r", "\n");

    // Walk VEVENT blocks
    int pos = 0;
    while (pos < (int)body.length()) {
        int begin = body.indexOf("BEGIN:VEVENT", pos);
        if (begin < 0) break;
        int end = body.indexOf("END:VEVENT", begin);
        if (end < 0) break;

        // Prefix with \n so icalProp() can match at the first line too
        String vevent = "\n" + body.substring(begin, end);

        String dtstart = icalProp(vevent, "DTSTART");
        if (dtstart.length() < 8) { pos = end + 10; continue; }

        // Compare date portion only (first 8 chars = YYYYMMDD)
        if (strncmp(dtstart.c_str(), todayStr, 8) != 0) { pos = end + 10; continue; }

        if (cal_task_count >= MAX_TASKS) break;
        CalTask &t = cal_tasks[cal_task_count];

        // ID: prefer UID, fall back to name+dtstart
        String uid = icalProp(vevent, "UID");
        if (uid.length() > 0)
            snprintf(t.id, sizeof(t.id), "ical_%s", uid.c_str());
        else
            snprintf(t.id, sizeof(t.id), "ical_%s_%s", entry.name, dtstart.c_str());
        t.id[sizeof(t.id) - 1] = '\0';

        // Title
        String summary = icalProp(vevent, "SUMMARY");
        if (summary.length() == 0) summary = "Untitled";
        strncpy(t.title, summary.c_str(), MAX_TITLE_LEN - 1);
        t.title[MAX_TITLE_LEN - 1] = '\0';

        // Time — extract HH:MM if DTSTART has a time component
        if (dtstart.length() >= 13 && dtstart[8] == 'T') {
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%c%c:%c%c",
                     dtstart[9], dtstart[10], dtstart[11], dtstart[12]);
            strncpy(t.time, timeBuf, sizeof(t.time) - 1);
        } else {
            strncpy(t.time, "All day", sizeof(t.time) - 1);
        }
        t.time[sizeof(t.time) - 1] = '\0';

        t.completed = isTaskCompletedForOffset(t, cal_day_offset);
        cal_task_count++;
        pos = end + 10;
    }
    return true;
}

// ══════════════════════════════════════
// MAIN ENTRY POINT
// ══════════════════════════════════════

bool fetchCalendarEvents() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Cal] No WiFi");
        return false;
    }
    struct tm now;
    if (!getLocalTime(&now)) {
        Serial.println("[Cal] No time sync");
        return false;
    }

    saveCurrentDayCompletionState();
    cal_task_count = 0;

    for (int c = 0; c < cal_entry_count; c++) {
        if      (strcmp(cal_entries[c].type, "gcal") == 0) fetchGCalForDay(cal_entries[c]);
        else if (strcmp(cal_entries[c].type, "ical") == 0) fetchICalForDay(cal_entries[c]);
    }

    if (cal_task_count == 0) {
        strncpy(cal_tasks[0].title, "No events", MAX_TITLE_LEN - 1);
        cal_tasks[0].title[MAX_TITLE_LEN - 1] = '\0';
        cal_tasks[0].id[0]   = '\0';
        cal_tasks[0].time[0] = '\0';
        cal_tasks[0].completed = false;
        cal_task_count = 1;
    }

    cal_loaded_day_offset = cal_day_offset;
    Serial.printf("[Cal] Total events: %d\n", cal_task_count);
    return true;
}

#endif // CALENDAR_FETCH_H
