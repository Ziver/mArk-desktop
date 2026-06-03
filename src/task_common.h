/**
 * Task Common — shared data structures, state, and helper functions.
 */

#ifndef TASK_COMMON_H
#define TASK_COMMON_H

#include <Arduino.h>
#include <time.h>
#include "task_store.h"

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

#endif // TASK_COMMON_H
