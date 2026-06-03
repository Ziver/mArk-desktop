/**
 * iCal / ICS Feed Task Provider.
 */

#ifndef PROVIDER_ICAL_H
#define PROVIDER_ICAL_H

#include <HTTPClient.h>
#include "task_common.h"

static bool fetchICalForDay(const TaskProvider& entry) {
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

#endif // PROVIDER_ICAL_H
