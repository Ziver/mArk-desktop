/**
 * Vikunja Task Provider.
 */

#ifndef PROVIDER_VIKUNJA_H
#define PROVIDER_VIKUNJA_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "task_common.h"

static String sanitizeVikunjaBaseUrl(const char* rawUrl) {
    String baseUrl = String(rawUrl);
    baseUrl.trim();
    while (baseUrl.endsWith("/")) {
        baseUrl.remove(baseUrl.length() - 1);
    }
    if (baseUrl.endsWith("/login")) {
        baseUrl.remove(baseUrl.length() - 6);
    } else if (baseUrl.endsWith("/register")) {
        baseUrl.remove(baseUrl.length() - 9);
    } else if (baseUrl.endsWith("/tasks")) {
        baseUrl.remove(baseUrl.length() - 6);
    } else if (baseUrl.endsWith("/lists")) {
        baseUrl.remove(baseUrl.length() - 6);
    } else if (baseUrl.endsWith("/projects")) {
        baseUrl.remove(baseUrl.length() - 9);
    }
    while (baseUrl.endsWith("/")) {
        baseUrl.remove(baseUrl.length() - 1);
    }
    return baseUrl;
}

static bool fetchVikunjaForDay(const TaskProvider& entry, int cal_idx) {
    struct tm now;
    if (!getLocalTime(&now)) return false;
    now.tm_mday += cal_day_offset;
    mktime(&now);

    char timeMin[16], timeMax[16];
    snprintf(timeMin, sizeof(timeMin), "%04d-%02d-%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    struct tm nextDay = now;
    nextDay.tm_mday += 1;
    mktime(&nextDay);
    snprintf(timeMax, sizeof(timeMax), "%04d-%02d-%02d",
             nextDay.tm_year + 1900, nextDay.tm_mon + 1, nextDay.tm_mday);

    String baseUrl = sanitizeVikunjaBaseUrl(entry.url);
    String filterEscaped = "due_date >= " + String(timeMin) + " && due_date < " + String(timeMax) +
                           " || start_date >= " + String(timeMin) + " && start_date < " + String(timeMax);
    filterEscaped.replace(" ", "%20");
    filterEscaped.replace("&", "%26");
    filterEscaped.replace(">", "%3E");
    filterEscaped.replace("<", "%3C");
    filterEscaped.replace("=", "%3D");
    filterEscaped.replace("|", "%7C");

    String url = baseUrl + "/api/v1/tasks?filter=" + filterEscaped;

    Serial.printf("[Vikunja] Fetching: %s\n", entry.name);
    Serial.printf("[Vikunja] URL: %s\n", url.c_str());

    HTTPClient http;
    WiFiClientSecure *secClient = nullptr;
    if (url.startsWith("https://")) {
        secClient = new WiFiClientSecure();
        secClient->setInsecure();
        http.begin(*secClient, url);
    } else {
        http.begin(url);
    }

    http.addHeader("Authorization", "Bearer " + String(entry.token));
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[Vikunja] HTTP %d\n", code);
        String errPayload = http.getString();
        Serial.printf("[Vikunja] Error payload: %s\n", errPayload.c_str());
        http.end();
        if (secClient) delete secClient;
        return false;
    }

    String payload = http.getString();
    http.end();
    if (secClient) delete secClient;

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Vikunja] JSON parse error: %s\n", err.c_str());
        Serial.printf("[Vikunja] Payload snippet: %.128s\n", payload.c_str());
        return false;
    }

    JsonArray items = doc.as<JsonArray>();
    for (JsonObject item : items) {
        if (cal_task_count >= MAX_TASKS) break;
        CalTask &t = cal_tasks[cal_task_count];

        long long id = item["id"] | 0;
        snprintf(t.id, sizeof(t.id), "vikunja_%d_%lld", cal_idx, id);

        const char* title = item["title"] | "Untitled";
        strncpy(t.title, title, MAX_TITLE_LEN - 1);
        t.title[MAX_TITLE_LEN - 1] = '\0';

        const char* due_date_str = item["due_date"];
        const char* start_date_str = item["start_date"];
        bool has_due = (due_date_str && strlen(due_date_str) >= 19 && strncmp(due_date_str, "0001-01-01", 10) != 0);
        bool has_start = (start_date_str && strlen(start_date_str) >= 19 && strncmp(start_date_str, "0001-01-01", 10) != 0);

        if (has_due) {
            parseTime(due_date_str, t.time, sizeof(t.time));
        } else if (has_start) {
            parseTime(start_date_str, t.time, sizeof(t.time));
        } else {
            snprintf(t.time, sizeof(t.time), "All day");
        }

        bool done = item["done"] | false;
        t.completed = done;

        if (!t.completed) {
            t.completed = isTaskCompletedForOffset(t, cal_day_offset);
        }

        cal_task_count++;
    }
    return true;
}

static void updateVikunjaTaskCompletion(const char* compositeId, bool completed) {
    int cal_idx = -1;
    long long task_id = 0;
    if (sscanf(compositeId, "vikunja_%d_%lld", &cal_idx, &task_id) != 2) return;
    if (cal_idx < 0 || cal_idx >= task_provider_count) return;

    const TaskProvider& entry = task_providers[cal_idx];
    String baseUrl = sanitizeVikunjaBaseUrl(entry.url);
    String url = baseUrl + "/api/v1/tasks/" + String((long)task_id);

    Serial.printf("[Vikunja] Updating task %lld completion -> %s\n", task_id, completed ? "done" : "undone");

    HTTPClient http;
    WiFiClientSecure *secClient = nullptr;
    if (url.startsWith("https://")) {
        secClient = new WiFiClientSecure();
        secClient->setInsecure();
        http.begin(*secClient, url);
    } else {
        http.begin(url);
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(entry.token));
    http.addHeader("Accept", "application/json");

    String json = "{\"done\":" + String(completed ? "true" : "false") + "}";
    int code = http.POST(json);

    if (code == 200 || code == 201) {
        Serial.printf("[Vikunja] Successfully updated task %lld\n", task_id);
    } else {
        Serial.printf("[Vikunja] Failed to update task %lld: HTTP %d\n", task_id, code);
    }
    http.end();
    if (secClient) delete secClient;
}

#endif // PROVIDER_VIKUNJA_H
