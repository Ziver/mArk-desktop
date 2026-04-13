/**
 * Calendar Store — persists calendar configs in NVS via Preferences.
 * Supports up to MAX_CALENDARS calendars (type "gcal" or "ical").
 */

#ifndef CALENDAR_STORE_H
#define CALENDAR_STORE_H

#include <Preferences.h>
#include <Arduino.h>

#define MAX_CALENDARS 5
#define CAL_NAME_LEN  32
#define CAL_URL_LEN   256

struct CalendarEntry {
    char type[8];           // "gcal" or "ical"
    char name[CAL_NAME_LEN];
    char url[CAL_URL_LEN];  // gcal: calendar email; ical: full https URL
};

static CalendarEntry cal_entries[MAX_CALENDARS];
static int cal_entry_count = 0;

static void calStoreLoad() {
    Preferences prefs;
    prefs.begin("cals", true);
    int cnt = prefs.getInt("cnt", 0);
    if (cnt > MAX_CALENDARS) cnt = MAX_CALENDARS;
    cal_entry_count = 0;
    for (int i = 0; i < cnt; i++) {
        char tk[4], nk[4], uk[4];
        snprintf(tk, sizeof(tk), "t%d", i);
        snprintf(nk, sizeof(nk), "n%d", i);
        snprintf(uk, sizeof(uk), "u%d", i);
        prefs.getString(tk, cal_entries[i].type, sizeof(cal_entries[i].type));
        prefs.getString(nk, cal_entries[i].name, sizeof(cal_entries[i].name));
        prefs.getString(uk, cal_entries[i].url,  sizeof(cal_entries[i].url));
        cal_entry_count++;
    }
    prefs.end();
}

static void calStoreSave() {
    Preferences prefs;
    prefs.begin("cals", false);
    prefs.putInt("cnt", cal_entry_count);
    for (int i = 0; i < cal_entry_count; i++) {
        char tk[4], nk[4], uk[4];
        snprintf(tk, sizeof(tk), "t%d", i);
        snprintf(nk, sizeof(nk), "n%d", i);
        snprintf(uk, sizeof(uk), "u%d", i);
        prefs.putString(tk, cal_entries[i].type);
        prefs.putString(nk, cal_entries[i].name);
        prefs.putString(uk, cal_entries[i].url);
    }
    prefs.end();
}

static bool calStoreAdd(const char* type, const char* name, const char* url) {
    if (cal_entry_count >= MAX_CALENDARS) return false;
    int i = cal_entry_count;
    strncpy(cal_entries[i].type, type, sizeof(cal_entries[i].type) - 1);
    cal_entries[i].type[sizeof(cal_entries[i].type) - 1] = '\0';
    strncpy(cal_entries[i].name, name, sizeof(cal_entries[i].name) - 1);
    cal_entries[i].name[sizeof(cal_entries[i].name) - 1] = '\0';
    strncpy(cal_entries[i].url,  url,  sizeof(cal_entries[i].url)  - 1);
    cal_entries[i].url[sizeof(cal_entries[i].url) - 1] = '\0';
    cal_entry_count++;
    calStoreSave();
    return true;
}

static bool calStoreDelete(int idx) {
    if (idx < 0 || idx >= cal_entry_count) return false;
    for (int i = idx; i < cal_entry_count - 1; i++)
        cal_entries[i] = cal_entries[i + 1];
    cal_entry_count--;
    calStoreSave();
    return true;
}

// Call once at startup — seeds the default Google Calendar on first run.
void calStoreInit() {
    Preferences prefs;
    prefs.begin("cals", false);
    int cnt = prefs.getInt("cnt", -1);
    if (cnt < 0) {
        // First run — write default calendar
        prefs.putInt("cnt", 1);
        prefs.putString("t0", "gcal");
        prefs.putString("n0", "My Calendar");
        prefs.putString("u0", "your.email@gmail.com");
    }
    prefs.end();
    calStoreLoad();
}

#endif // CALENDAR_STORE_H
