/**
 * Display driver for Elecrow CrowPanel 5.0" ESP32-S3 (Basic/V1-V2)
 * 800x480 RGB with GT911 capacitive touch
 * 
 * Pin config from official Elecrow 5" example code.
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lvgl.h>

// Display resolution
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// Backlight pin
#define TFT_BL 2

// ============================================
// LovyanGFX display configuration
// Matches Elecrow CrowPanel 5.0" Basic example
// ============================================
class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Panel_RGB   _panel_instance;

    LGFX(void) {
        // --- Bus (RGB parallel) ---
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0  = GPIO_NUM_8;   // B0
            cfg.pin_d1  = GPIO_NUM_3;   // B1
            cfg.pin_d2  = GPIO_NUM_46;  // B2
            cfg.pin_d3  = GPIO_NUM_9;   // B3
            cfg.pin_d4  = GPIO_NUM_1;   // B4

            cfg.pin_d5  = GPIO_NUM_5;   // G0
            cfg.pin_d6  = GPIO_NUM_6;   // G1
            cfg.pin_d7  = GPIO_NUM_7;   // G2
            cfg.pin_d8  = GPIO_NUM_15;  // G3
            cfg.pin_d9  = GPIO_NUM_16;  // G4
            cfg.pin_d10 = GPIO_NUM_4;   // G5

            cfg.pin_d11 = GPIO_NUM_45;  // R0
            cfg.pin_d12 = GPIO_NUM_48;  // R1
            cfg.pin_d13 = GPIO_NUM_47;  // R2
            cfg.pin_d14 = GPIO_NUM_21;  // R3
            cfg.pin_d15 = GPIO_NUM_14;  // R4

            // 5.0" Basic: henable=40, vsync=41, hsync=39
            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync   = GPIO_NUM_41;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_0;
            cfg.freq_write  = 15000000;  // Official 5" basic timing

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;

            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;

            cfg.pclk_active_neg   = 1;
            cfg.de_idle_high      = 0;
            cfg.pclk_idle_high    = 0;

            _bus_instance.config(cfg);
        }

        // --- Panel (800x480) ---
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = SCREEN_WIDTH;
            cfg.memory_height = SCREEN_HEIGHT;
            cfg.panel_width   = SCREEN_WIDTH;
            cfg.panel_height  = SCREEN_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.rgb_order = true;  // Swap R↔B channels for 5" basic panel
            _panel_instance.config(cfg);
        }

        _panel_instance.setBus(&_bus_instance);
        setPanel(&_panel_instance);
    }
};

// ============================================
// LVGL integration
// ============================================
static LGFX lcd;
static lv_disp_draw_buf_t draw_buf;

// Use a smaller buffer to avoid BSS overflow
#define DISP_BUF_SIZE (SCREEN_WIDTH * 20)
static lv_color_t disp_draw_buf[DISP_BUF_SIZE];

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area,
                           lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)color_p);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

void setupDisplay() {
    // Basic 5" board: no PCA9557, just init I2C for touch
    Wire.begin(19, 20);
    delay(50);

    Serial.println("[DISP] lcd.begin()...");
    lcd.begin();
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextSize(2);
    delay(200);

    Serial.println("[DISP] lv_init()...");
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("[DISP] Backlight...");
    // Backlight via LEDC PWM
    ledcSetup(1, 300, 8);
    ledcAttachPin(TFT_BL, 1);
    ledcWrite(1, 255);

    Serial.println("[DISP] Display ready!");
}

static bool display_sleeping = false;
static unsigned long last_activity_time = 0;

void setDisplaySleep(bool sleep) {
    display_sleeping = sleep;
    ledcWrite(1, sleep ? 0 : 255);
    if (!sleep) {
        last_activity_time = millis();
    }
    Serial.printf("[DISP] Sleep: %s\n", sleep ? "ON" : "OFF");
}

#endif /* DISPLAY_DRIVER_H */
