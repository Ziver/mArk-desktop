/**
 * Google Calendar Task Provider.
 */

#ifndef PROVIDER_GCAL_H
#define PROVIDER_GCAL_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "task_common.h"
#include "credentials.h"

static bool fetchGCalForDay(const TaskProvider& entry) {
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

#endif // PROVIDER_GCAL_H
