/*******************************************************************************
 * Touch driver for CrowPanel 7.0" — uses TAMC_GT911 directly
 * Based on official Elecrow example code
 * Requires display_driver.h to be included BEFORE this file
 ******************************************************************************/

#ifndef TOUCH_H
#define TOUCH_H

#include <Wire.h>
#include <TAMC_GT911.h>

// GT911 config matching Elecrow example
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST -1
#define TOUCH_GT911_ROTATION ROTATION_NORMAL
#define TOUCH_MAP_X1 800
#define TOUCH_MAP_X2 0
#define TOUCH_MAP_Y1 480
#define TOUCH_MAP_Y2 0

static TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, SCREEN_WIDTH, SCREEN_HEIGHT);

static int touch_last_x = 0;
static int touch_last_y = 0;

void touch_init() {
    Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
    ts.begin();
    ts.setRotation(TOUCH_GT911_ROTATION);
    Serial.println("[TOUCH] TAMC_GT911 initialized");
}

bool touch_has_signal() {
    return true;
}

bool touch_touched() {
    ts.read();
    if (ts.isTouched) {
        touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 800 - 1);
        touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 480 - 1);
        return true;
    }
    return false;
}

bool touch_released() {
    return true;
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    (void)indev_driver;
    if (touch_has_signal()) {
        if (touch_touched()) {
            if (display_sleeping) {
                setDisplaySleep(false);
                data->state = LV_INDEV_STATE_REL;
                return;
            }
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            static unsigned long lastLog = 0;
            if (millis() - lastLog > 300) {
                Serial.printf("[TOUCH] x=%d y=%d\n", touch_last_x, touch_last_y);
                lastLog = millis();
            }
        } else if (touch_released()) {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

#endif /* TOUCH_H */
