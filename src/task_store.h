/**
 * Task Store — persists task configs in NVS via Preferences.
 * Supports up to MAX_PROVIDERS providers (type "gcal", "ical", or "vikunja").
 */

#ifndef TASK_STORE_H
#define TASK_STORE_H

#include <Preferences.h>
#include <Arduino.h>

#define MAX_PROVIDERS 5
#define TASK_NAME_LEN  32
#define TASK_URL_LEN   256
#define TASK_TOKEN_LEN 128

struct TaskProvider {
    char type[8];           // "gcal", "ical", or "vikunja"
    char name[TASK_NAME_LEN];
    char url[TASK_URL_LEN];  // gcal: email; ical: full URL; vikunja: base URL
    char token[TASK_TOKEN_LEN]; // vikunja token
};

static TaskProvider task_providers[MAX_PROVIDERS];
static int task_provider_count = 0;

static void taskStoreLoad() {
    Preferences prefs;
    prefs.begin("tasks", true);
    int cnt = prefs.getInt("cnt", 0);
    if (cnt > MAX_PROVIDERS) cnt = MAX_PROVIDERS;
    task_provider_count = 0;
    for (int i = 0; i < cnt; i++) {
        char tk[4], nk[4], uk[4], kk[4];
        snprintf(tk, sizeof(tk), "t%d", i);
        snprintf(nk, sizeof(nk), "n%d", i);
        snprintf(uk, sizeof(uk), "u%d", i);
        snprintf(kk, sizeof(kk), "k%d", i);
        prefs.getString(tk, task_providers[i].type, sizeof(task_providers[i].type));
        prefs.getString(nk, task_providers[i].name, sizeof(task_providers[i].name));
        prefs.getString(uk, task_providers[i].url,  sizeof(task_providers[i].url));
        prefs.getString(kk, task_providers[i].token, sizeof(task_providers[i].token));
        task_provider_count++;
    }
    prefs.end();
}

static void taskStoreSave() {
    Preferences prefs;
    prefs.begin("tasks", false);
    prefs.putInt("cnt", task_provider_count);
    for (int i = 0; i < task_provider_count; i++) {
        char tk[4], nk[4], uk[4], kk[4];
        snprintf(tk, sizeof(tk), "t%d", i);
        snprintf(nk, sizeof(nk), "n%d", i);
        snprintf(uk, sizeof(uk), "u%d", i);
        snprintf(kk, sizeof(kk), "k%d", i);
        prefs.putString(tk, task_providers[i].type);
        prefs.putString(nk, task_providers[i].name);
        prefs.putString(uk, task_providers[i].url);
        prefs.putString(kk, task_providers[i].token);
    }
    prefs.end();
}

static bool taskStoreAdd(const char* type, const char* name, const char* url, const char* token) {
    if (task_provider_count >= MAX_PROVIDERS) return false;
    int i = task_provider_count;
    strncpy(task_providers[i].type, type, sizeof(task_providers[i].type) - 1);
    task_providers[i].type[sizeof(task_providers[i].type) - 1] = '\0';
    strncpy(task_providers[i].name, name, sizeof(task_providers[i].name) - 1);
    task_providers[i].name[sizeof(task_providers[i].name) - 1] = '\0';
    strncpy(task_providers[i].url,  url,  sizeof(task_providers[i].url)  - 1);
    task_providers[i].url[sizeof(task_providers[i].url) - 1] = '\0';
    strncpy(task_providers[i].token, token, sizeof(task_providers[i].token) - 1);
    task_providers[i].token[sizeof(task_providers[i].token) - 1] = '\0';
    task_provider_count++;
    taskStoreSave();
    return true;
}

static bool taskStoreDelete(int idx) {
    if (idx < 0 || idx >= task_provider_count) return false;
    for (int i = idx; i < task_provider_count - 1; i++)
        task_providers[i] = task_providers[i + 1];
    task_provider_count--;
    taskStoreSave();
    return true;
}

static bool taskStoreGetDarkMode() {
    Preferences prefs;
    prefs.begin("tasks", true);
    bool dark = prefs.getBool("dark", false);
    prefs.end();
    return dark;
}

static void taskStoreSetDarkMode(bool dark) {
    Preferences prefs;
    prefs.begin("tasks", false);
    prefs.putBool("dark", dark);
    prefs.end();
}

// Call once at startup — seeds default or migrates old config.
inline void taskStoreInit() {
    Preferences prefs;
    prefs.begin("tasks", false);
    int cnt = prefs.getInt("cnt", -1);
    if (cnt < 0) {
        // Try migrating from old "cals" Preferences
        Preferences oldPrefs;
        oldPrefs.begin("cals", true);
        int oldCnt = oldPrefs.getInt("cnt", -1);
        if (oldCnt >= 0) {
            Serial.println("[Store] Migrating from old 'cals' preferences namespace...");
            prefs.putInt("cnt", oldCnt);
            for (int i = 0; i < oldCnt; i++) {
                char tk[4], nk[4], uk[4], kk[4];
                snprintf(tk, sizeof(tk), "t%d", i);
                snprintf(nk, sizeof(nk), "n%d", i);
                snprintf(uk, sizeof(uk), "u%d", i);
                snprintf(kk, sizeof(kk), "k%d", i);
                String t = oldPrefs.getString(tk, "");
                String n = oldPrefs.getString(nk, "");
                String u = oldPrefs.getString(uk, "");
                prefs.putString(tk, t);
                prefs.putString(nk, n);
                prefs.putString(uk, u);
                prefs.putString(kk, "");
            }
            oldPrefs.end();
            // Clear old namespace
            Preferences oldPrefsWrite;
            oldPrefsWrite.begin("cals", false);
            oldPrefsWrite.clear();
            oldPrefsWrite.end();
        } else {
            // First run — write default
            prefs.putInt("cnt", 1);
            prefs.putString("t0", "gcal");
            prefs.putString("n0", "My Tasks");
            prefs.putString("u0", "your.email@gmail.com");
            prefs.putString("k0", "");
        }
    }
    prefs.end();
    taskStoreLoad();
}

#endif // TASK_STORE_H
