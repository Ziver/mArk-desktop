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
#include "task_store.h"
#include "task_fetch.h"
#include "streak_store.h"
// sound_driver.h removed — I2S conflicts with RGB display on arduino-esp32 2.0.3
#include "ui_taskviewer.h"
#include "web_settings.h"

StreakStore streakStore;

static lv_obj_t *loading_title = nullptr;
static lv_obj_t *loading_sub = nullptr;
static lv_obj_t *loading_bar = nullptr;

static void refresh_lvgl() {
    static uint32_t last_tick_ms = 0;
    uint32_t now = millis();
    if (last_tick_ms == 0) {
        last_tick_ms = now;
    }
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;
    lv_timer_handler();
}

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

    // Initialize dark mode preference early for the loading screen
    ui_dark_mode = taskStoreGetDarkMode();

    // Show loading screen matching theme preference
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, ui_dark_mode ? C_DARK_BG : C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Large title
    loading_title = lv_label_create(scr);
    lv_label_set_text(loading_title, "Initializing...");
    lv_obj_set_style_text_color(loading_title, ui_dark_mode ? C_DARK_TEXT : C_FG, 0);
    lv_obj_set_style_text_font(loading_title, &lv_font_montserrat_28, 0);
    lv_obj_align(loading_title, LV_ALIGN_CENTER, 0, -40);

    // Sublabel for status
    loading_sub = lv_label_create(scr);
    lv_label_set_text(loading_sub, "Initializing connection...");
    lv_obj_set_style_text_color(loading_sub, ui_dark_mode ? C_DARK_MUTED : C_MUTED, 0);
    lv_obj_set_style_text_font(loading_sub, &lv_font_montserrat_14, 0);
    lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);

    // Accent-colored progress bar
    loading_bar = lv_bar_create(scr);
    lv_obj_set_size(loading_bar, 300, 6);
    lv_obj_align(loading_bar, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_style_bg_color(loading_bar, ui_dark_mode ? C_DARK_BORDER : C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(loading_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(loading_bar, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(loading_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(loading_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(loading_bar, 3, LV_PART_INDICATOR);
    lv_bar_set_value(loading_bar, 10, LV_ANIM_OFF);

    refresh_lvgl();
    delay(500); // Allow initial screen to render fully

    // 2. WiFi + NTP
    bool online = setupWiFi([](int attempts, const char* status) {
        Serial.printf("[SETUP_CB] attempts=%d, status=%s\n", attempts, status);
        if (loading_title && loading_sub && loading_bar) {
            if (attempts >= 0) {
                lv_label_set_text(loading_title, "Initializing...");
                lv_obj_align(loading_title, LV_ALIGN_CENTER, 0, -40);
                lv_label_set_text(loading_sub, status);
                lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);
                lv_bar_set_value(loading_bar, 10 + attempts, LV_ANIM_OFF);
            } else if (attempts == -1) {
                lv_label_set_text(loading_title, "Initializing...");
                lv_obj_align(loading_title, LV_ALIGN_CENTER, 0, -40);
                lv_label_set_text(loading_sub, status);
                lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);
                lv_bar_set_value(loading_bar, 60, LV_ANIM_OFF);
            } else if (attempts == -2) {
                lv_label_set_text(loading_title, "Failed to Connect");
                lv_obj_set_style_text_color(loading_title, lv_color_hex(0xE53E3E), 0); // Red error text
                lv_obj_align(loading_title, LV_ALIGN_CENTER, 0, -40);
                lv_label_set_text(loading_sub, status);
                lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);
                lv_bar_set_value(loading_bar, 100, LV_ANIM_OFF);
            }
            refresh_lvgl();
        }
    });

    // 3. Streak store + calendar store (NVS)
    streakStore.begin();
    taskStoreInit();

    // 4. Web settings server
    if (online) webSettingsSetup();

    // 5. Fetch tasks
    if (online) {
        if (loading_title) {
            lv_label_set_text(loading_title, "Initializing...");
            lv_obj_align(loading_title, LV_ALIGN_CENTER, 0, -40);
        }
        if (loading_sub) {
            lv_label_set_text(loading_sub, "Fetching tasks from providers...");
            lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);
        }
        if (loading_bar) lv_bar_set_value(loading_bar, 85, LV_ANIM_OFF);
        refresh_lvgl();
        delay(800); // Allow NTP sync info to stay on screen
        fetchTasks();
        if (loading_bar) lv_bar_set_value(loading_bar, 100, LV_ANIM_OFF);
        if (loading_sub) {
            lv_label_set_text(loading_sub, "Ready!");
            lv_obj_align(loading_sub, LV_ALIGN_CENTER, 0, 10);
        }
        refresh_lvgl();
        delay(500); // Pause briefly at 100% loading
    } else {
        // Offline fallback
        strncpy(cal_tasks[0].title, "No WiFi connection", MAX_TITLE_LEN);
        cal_tasks[0].time[0] = '\0';
        cal_tasks[0].completed = false;
        cal_task_count = 1;
        delay(3000); // Give user time to see the red "Failed to Connect" screen
    }
    
    // 6. Clear entire screen and build UI
    uiRebuild(false);

    // Force full screen redraw
    lv_refr_now(NULL);
    last_activity_time = millis();
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

    // Inactivity screen timeout check
    if (!display_sleeping) {
        int timeout_min = taskStoreGetScreenTimeout();
        if (timeout_min > 0) {
            unsigned long timeout_ms = (unsigned long)timeout_min * 60 * 1000;
            if (millis() - last_activity_time > timeout_ms) {
                Serial.printf("[Timeout] Screen inactive for %d min, turning off\n", timeout_min);
                setDisplaySleep(true);
            }
        }
    }

    // Detect wake-up from sleep → reconnect WiFi + refresh tasks
    if (was_sleeping && !display_sleeping) {
        Serial.println("[Wake] Display woke up, checking WiFi...");
        if (ensureWiFiConnected()) {
            webSettingsSetup();  // restart web server after reconnect
            trigger_task_refresh();
            lastRefresh = millis();
        }
    }
    was_sleeping = display_sleeping;

    // Periodic WiFi health check
    if (millis() - lastWifiCheck > WIFI_CHECK_MS) {
        if (!display_sleeping) ensureWiFiConnected();
        lastWifiCheck = millis();
    }

    // Periodic task refresh
    if (millis() - lastRefresh > REFRESH_INTERVAL_MS && wifi_connected && !display_sleeping) {
        Serial.println("[Auto] Refreshing tasks...");
        trigger_task_refresh();
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
