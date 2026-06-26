/**
 * Task Fetcher — includes specific task provider implementations.
 */

#ifndef TASK_FETCH_H
#define TASK_FETCH_H

#include "task_common.h"
#include "providers/provider_gcal.h"
#include "providers/provider_ical.h"
#include "providers/provider_vikunja.h"

// ══════════════════════════════════════
// MAIN ENTRY POINT
// ══════════════════════════════════════

bool fetchTasks() {
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

    for (int c = 0; c < task_provider_count; c++) {
        if      (strcmp(task_providers[c].type, "gcal") == 0) fetchGCalForDay(task_providers[c]);
        else if (strcmp(task_providers[c].type, "ical") == 0) fetchICalForDay(task_providers[c]);
        else if (strcmp(task_providers[c].type, "vikunja") == 0) fetchVikunjaForDay(task_providers[c], c);
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

#endif // TASK_FETCH_H
