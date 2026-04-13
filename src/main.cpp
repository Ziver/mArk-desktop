/**
 * Task Viewer — CrowPanel 5.0" ESP32-S3
 * 
 * Touch-only: WiFi → NTP → Google Calendar → LVGL UI
 * No rotary encoder, no LEDs, no audio.
 */

#include <Arduino.h>
#include "display_driver.h"
#include "touch.h"
#include "wifi_manager.h"
#include "calendar_store.h"
#include "calendar_fetch.h"
#include "web_settings.h"
#include "streak_store.h"
// sound_driver.h removed — I2S conflicts with RGB display on arduino-esp32 2.0.3
#include "ui_taskviewer.h"

StreakStore streakStore;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("=============================");
    Serial.println("  TASK VIEWER v3.2 (5\" Touch)");
    Serial.println("=============================");
    Serial.println();

    // 1. Display + Touch
    setupDisplay();
    Serial.println("[OK] Display ready");

    // Register touch input device with LVGL
    touch_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[OK] Touch ready");

    // Sound init deferred until after display + UI is fully built

    // Show loading screen
    lv_obj_t *loading = lv_label_create(lv_scr_act());
    lv_label_set_text(loading, "Connecting...");
    lv_obj_set_style_text_color(loading, lv_color_hex(0x6E7080), 0);
    lv_obj_set_style_text_font(loading, &font_swedish_14, 0);
    lv_obj_center(loading);
    lv_timer_handler();

    // 2. WiFi + NTP
    bool online = setupWiFi();
    
    // 3. Streak store + calendar store (NVS)
    streakStore.begin();
    calStoreInit();

    // 4. Web settings server
    if (online) webSettingsSetup();

    // 5. Fetch calendar events
    if (online) {
        lv_label_set_text(loading, "Loading calendar...");
        lv_timer_handler();
        fetchCalendarEvents();
    } else {
        // Offline fallback
        strncpy(cal_tasks[0].title, "No WiFi connection", MAX_TITLE_LEN);
        cal_tasks[0].time[0] = '\0';
        cal_tasks[0].completed = false;
        cal_task_count = 1;
    }
    
    // 6. Clear entire screen and build UI
    lv_obj_clean(lv_scr_act());
    buildTaskViewerUI();
    
    // Force full screen redraw
    lv_refr_now(NULL);
    Serial.println("[OK] Setup complete!");
}

unsigned long lastRefresh     = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastWifiCheck   = 0;
#define REFRESH_INTERVAL_MS   (5  * 60 * 1000)
#define CLOCK_UPDATE_MS       10000
#define WIFI_CHECK_MS         (30 * 1000)

int counter = 0;

void loop() {
    static uint32_t last_ms  = 0;
    static bool     was_sleeping = false;
    uint32_t now = millis();
    if (last_ms == 0) last_ms = now;
    lv_tick_inc(now - last_ms);
    last_ms = now;

    lv_timer_handler();
    webSettingsLoop();

    delay(5);

    // Detect wake-up from sleep → reconnect WiFi + refresh calendar
    if (was_sleeping && !display_sleeping) {
        Serial.println("[Wake] Display woke up, checking WiFi...");
        if (ensureWiFiConnected()) {
            webSettingsSetup();  // restart web server after reconnect
            fetchCalendarEvents();
            ui_completed = 0;
            for (int i = 0; i < cal_task_count; i++)
                if (cal_tasks[i].completed) ui_completed++;
            do_refresh_all();
            lastRefresh = millis();
        }
    }
    was_sleeping = display_sleeping;

    // Periodic WiFi health check
    if (millis() - lastWifiCheck > WIFI_CHECK_MS) {
        if (!display_sleeping) ensureWiFiConnected();
        lastWifiCheck = millis();
    }

    // Periodic calendar refresh
    if (millis() - lastRefresh > REFRESH_INTERVAL_MS && wifi_connected && !display_sleeping) {
        Serial.println("[Auto] Refreshing calendar...");
        fetchCalendarEvents();
        ui_completed = 0;
        for (int i = 0; i < cal_task_count; i++)
            if (cal_tasks[i].completed) ui_completed++;
        do_refresh_all();
        lastRefresh = millis();
    }

    // Clock update every 10 seconds
    if (millis() - lastClockUpdate > CLOCK_UPDATE_MS) {
        refresh_clock();
        lastClockUpdate = millis();
    }

    if (counter % 2000 == 0) {
        Serial.printf("[LOOP] count=%d, heap=%u\n", counter, ESP.getFreeHeap());
    }
    counter++;
}
