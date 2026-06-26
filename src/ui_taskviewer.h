/**
 * Task Viewer UI — LVGL for CrowPanel 5.0" (800x480)
 * Touch-only. Ported from ESP-IDF version with keyboard + dark mode.
 */

#ifndef UI_TASKVIEWER_H
#define UI_TASKVIEWER_H

#include <lvgl.h>
// sound_driver.h removed — I2S conflicts with RGB LCD on arduino-esp32 2.0.3
#include "task_fetch.h"

// Swedish font extensions (Montserrat + åäöÅÄÖ, fallback to montserrat for other glyphs)
LV_FONT_DECLARE(font_swedish_14);
LV_FONT_DECLARE(font_swedish_28);
LV_FONT_DECLARE(font_swedish_48);
#include "streak_store.h"
#include "wifi_manager.h"

// ── Colors (Dieter Rams warm neutral palette) ──
#define C_BG           lv_color_hex(0xF5F0E8)
#define C_FG           lv_color_hex(0x2C2C2C)
#define C_MUTED        lv_color_hex(0x8A8A7A)
#define C_ACCENT       lv_color_hex(0xE8742A)
#define C_SIDEBAR      lv_color_hex(0xE8E6E2)
#define C_CARD         lv_color_hex(0xDFDDD9)
#define C_BORDER       lv_color_hex(0xD0CECC)
#define C_WHITE        lv_color_hex(0xFFFFFF)
#define C_TRACK        lv_color_hex(0xD5CFC5)
#define C_DARK_BG      lv_color_hex(0x1E1E1E)
#define C_DARK_TEXT    lv_color_hex(0xE8E0D8)
#define C_DARK_MUTED   lv_color_hex(0x9CA3AF)
#define C_DARK_SIDEBAR lv_color_hex(0x262626)
#define C_DARK_CARD    lv_color_hex(0x333333)
#define C_DARK_BORDER  lv_color_hex(0x444444)
#define C_DARK_TRACK   lv_color_hex(0x444444)

// ── Layout ──
#define SCREEN_W  800
#define SCREEN_H  480
#define SIDEBAR_W 240
#define MAIN_W    (SCREEN_W - SIDEBAR_W)

extern StreakStore streakStore;

// ── UI state ──
static int ui_current = 0;
static int ui_completed = 0;
static bool ui_dark_mode = false;
static bool ui_list_view = false;
static bool ui_showing_complete = false;
static bool ui_showing_keyboard = false;
static bool ui_refreshing = false;
static lv_obj_t *btn_refresh_sb = nullptr;
static lv_obj_t *lbl_refresh_sb = nullptr;

// ── Theme-aware color helpers ──
static inline lv_color_t th_bg()      { return ui_dark_mode ? C_DARK_BG : C_BG; }
static inline lv_color_t th_fg()      { return ui_dark_mode ? C_DARK_TEXT : C_FG; }
static inline lv_color_t th_muted()   { return ui_dark_mode ? C_DARK_MUTED : C_MUTED; }
static inline lv_color_t th_sidebar() { return ui_dark_mode ? C_DARK_SIDEBAR : C_SIDEBAR; }
static inline lv_color_t th_card()    { return ui_dark_mode ? C_DARK_CARD : C_CARD; }
static inline lv_color_t th_border()  { return ui_dark_mode ? C_DARK_BORDER : C_BORDER; }
static inline lv_color_t th_track()   { return ui_dark_mode ? C_DARK_TRACK : C_TRACK; }

// ── UI elements ──
static lv_obj_t *lbl_greeting, *lbl_date;
static lv_obj_t *lbl_streak_num, *lbl_streak_label;
static lv_obj_t *lbl_level_name, *lbl_level_next;
static lv_obj_t *arc_progress, *lbl_arc_num, *lbl_arc_total;
static lv_obj_t *bar_xp;
static lv_obj_t *lbl_task_counter, *lbl_task_time, *lbl_task_title;
static lv_obj_t *badge_completed;
static lv_obj_t *btn_complete, *lbl_btn_complete;
static lv_obj_t *dots_container;
static lv_obj_t *s_mp = NULL;
static lv_obj_t *lbl_clock = NULL;
static lv_obj_t *complete_screen = NULL;
static lv_obj_t *settings_overlay = NULL;

// ── On-screen keyboard state ──
#define KB_MAX_INPUT 128
static char kb_input[KB_MAX_INPUT];
static int  kb_input_len = 0;
static bool kb_shift = true;
static int  kb_mode = 0;  /* 0=lower, 1=upper, 2=numeric */
static lv_obj_t *kb_overlay = NULL;
static lv_obj_t *kb_lbl_input = NULL;

// ── Forward declarations ──
void refresh_task();
void refresh_progress();
void refresh_dots();
void refresh_sidebar();
void refresh_clock();
void show_complete_screen();
void hide_complete_screen();
void do_refresh_all();
void trigger_task_refresh();
static void show_keyboard();
static void hide_keyboard();
void buildTaskViewerUI();
void show_settings_overlay();
void hide_settings_overlay();
void uiRebuild(bool force_reopen_settings = false);

// ── Callbacks ──
static void cb_complete(lv_event_t *e) {
    if (cal_task_count <= 0) return;
    bool was = cal_tasks[ui_current].completed;
    cal_tasks[ui_current].completed = !was;

    // Sync completion status back to Vikunja if applicable
    if (strncmp(cal_tasks[ui_current].id, "vikunja_", 8) == 0) {
        updateVikunjaTaskCompletion(cal_tasks[ui_current].id, cal_tasks[ui_current].completed);
    }

    ui_completed = 0;
    for (int i = 0; i < cal_task_count; i++)
        if (cal_tasks[i].completed) ui_completed++;

    if (ui_completed == cal_task_count && cal_task_count > 0) {
        streakStore.markDayComplete(cal_day_offset);
        show_complete_screen();
    } else if (!cal_tasks[ui_current].completed) {
        // Task was un-completed — stay on it
    } else {
        // Task was completed — jump to next incomplete
        for (int i = 1; i < cal_task_count; i++) {
            int idx = (ui_current + i) % cal_task_count;
            if (!cal_tasks[idx].completed) {
                ui_current = idx;
                break;
            }
        }
    }

    refresh_task();
    refresh_progress();
    refresh_dots();
    refresh_sidebar();
}

static void cb_next(lv_event_t *e) {
    if (cal_task_count <= 0) return;
    ui_current = (ui_current + 1) % cal_task_count;
    refresh_task();
    refresh_dots();
}

static void cb_prev(lv_event_t *e) {
    if (cal_task_count <= 0) return;
    ui_current = (ui_current - 1 + cal_task_count) % cal_task_count;
    refresh_task();
    refresh_dots();
}

static void cb_gesture(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) cb_next(e);
    else if (dir == LV_DIR_RIGHT) cb_prev(e);
}

static void cb_toggle_dark(lv_event_t *e) {
    ui_dark_mode = !ui_dark_mode;
    taskStoreSetDarkMode(ui_dark_mode);
    Serial.printf("[UI] Dark mode: %s\n", ui_dark_mode ? "ON" : "OFF");
    uiRebuild(true);
}

static void cb_toggle_view(lv_event_t *e) {
    ui_list_view = !ui_list_view;
    taskStoreSetListView(ui_list_view);
    Serial.printf("[UI] List view: %s\n", ui_list_view ? "ON" : "OFF");
    uiRebuild(true);
}

static void cb_list_toggle(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= cal_task_count) return;

    bool was = cal_tasks[idx].completed;
    cal_tasks[idx].completed = !was;

    // Sync completion status back to Vikunja if applicable
    if (strncmp(cal_tasks[idx].id, "vikunja_", 8) == 0) {
        updateVikunjaTaskCompletion(cal_tasks[idx].id, cal_tasks[idx].completed);
    }

    ui_completed = 0;
    for (int i = 0; i < cal_task_count; i++) {
        if (cal_tasks[i].completed) ui_completed++;
    }

    if (ui_completed == cal_task_count && cal_task_count > 0) {
        streakStore.markDayComplete(cal_day_offset);
        show_complete_screen();
    }

    uiRebuild();
}

// ── Completion screen callbacks ──
static void cb_back_to_tasks(lv_event_t *e) {
    hide_complete_screen();
}

static void cb_auto_dismiss(lv_timer_t *t) {
    hide_complete_screen();
    lv_timer_del(t);
}

// ══════════════════════════════════════
// ON-SCREEN KEYBOARD
// ══════════════════════════════════════

static const char *kb_rows_lower[] = {
    "q","w","e","r","t","y","u","i","o","p",NULL,
    "a","s","d","f","g","h","j","k","l",NULL,
    "\x01","z","x","c","v","b","n","m","\x02",NULL,
    "\x03"," ","\x04",NULL,
};
static const char *kb_rows_upper[] = {
    "Q","W","E","R","T","Y","U","I","O","P",NULL,
    "A","S","D","F","G","H","J","K","L",NULL,
    "\x01","Z","X","C","V","B","N","M","\x02",NULL,
    "\x03"," ","\x04",NULL,
};
static const char *kb_rows_num[] = {
    "1","2","3","4","5","6","7","8","9","0",NULL,
    "-","/",":",";","(",")","&","@","\"",NULL,
    ".","?","!","'",",","\x02",NULL,
    "\x05"," ","\x04",NULL,
};

static void kb_update_display() {
    if (!kb_lbl_input) return;
    if (kb_input_len == 0) {
        lv_label_set_text(kb_lbl_input, "Task name...");
        lv_obj_set_style_text_color(kb_lbl_input, C_MUTED, 0);
    } else {
        lv_label_set_text(kb_lbl_input, kb_input);
        lv_obj_set_style_text_color(kb_lbl_input, th_fg(), 0);
    }
}

static void kb_add_local_task() {
    if (kb_input_len == 0) return;

    if (cal_task_count < MAX_TASKS) {
        CalTask &ct = cal_tasks[cal_task_count];
        snprintf(ct.id, sizeof(ct.id), "local-%lu", (unsigned long)millis());
        strncpy(ct.title, kb_input, MAX_TITLE_LEN - 1);
        ct.title[MAX_TITLE_LEN - 1] = '\0';
        ct.time[0] = '\0';
        ct.completed = false;
        cal_task_count++;

        // If we were showing "No events" placeholder, replace it
        if (cal_task_count == 2 && strcmp(cal_tasks[0].title, "No events") == 0) {
            cal_tasks[0] = cal_tasks[1];
            cal_task_count = 1;
        }

        Serial.printf("[UI] Added task: %s\n", kb_input);
        do_refresh_all();
    }
    hide_keyboard();
}

static void cb_kb_key(lv_event_t *e) {
    const char *key = (const char *)lv_event_get_user_data(e);

    if (key[0] == '\x01') {
        // Shift toggle
        kb_shift = !kb_shift;
        if (kb_mode != 2) kb_mode = kb_shift ? 1 : 0;
        hide_keyboard();
        show_keyboard();
    } else if (key[0] == '\x02') {
        // Backspace — handle multi-byte UTF-8
        if (kb_input_len > 0) {
            do { kb_input_len--; } while (kb_input_len > 0 && (kb_input[kb_input_len] & 0xC0) == 0x80);
            kb_input[kb_input_len] = '\0';
            kb_update_display();
        }
    } else if (key[0] == '\x03') {
        // 123 mode
        kb_mode = 2;
        hide_keyboard();
        show_keyboard();
    } else if (key[0] == '\x04') {
        // Submit
        kb_add_local_task();
    } else if (key[0] == '\x05') {
        // abc mode
        kb_mode = kb_shift ? 1 : 0;
        hide_keyboard();
        show_keyboard();
    } else if (key[0] == ' ') {
        if (kb_input_len < KB_MAX_INPUT - 1) {
            kb_input[kb_input_len++] = ' ';
            kb_input[kb_input_len] = '\0';
            kb_update_display();
        }
    } else {
        // Regular character (may be multi-byte UTF-8)
        int klen = strlen(key);
        if (kb_input_len + klen < KB_MAX_INPUT - 1) {
            memcpy(kb_input + kb_input_len, key, klen);
            kb_input_len += klen;
            kb_input[kb_input_len] = '\0';
            if (kb_shift && kb_mode != 2) {
                kb_shift = false;
                kb_mode = 0;
            }
            kb_update_display();
        }
    }
}

static void cb_kb_close(lv_event_t *e) {
    hide_keyboard();
}

static void cb_add_task(lv_event_t *e) {
    kb_input[0] = '\0';
    kb_input_len = 0;
    kb_shift = true;
    kb_mode = 1;  // Start uppercase
    show_keyboard();
}

static void show_keyboard() {
    if (kb_overlay) {
        lv_obj_del(kb_overlay);
        kb_overlay = NULL;
    }
    ui_showing_keyboard = true;

    lv_obj_t *scr = lv_scr_act();
    kb_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(kb_overlay);
    lv_obj_set_size(kb_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(kb_overlay, 0, 0);
    lv_obj_set_style_bg_color(kb_overlay, th_bg(), 0);
    lv_obj_set_style_bg_opa(kb_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(kb_overlay, LV_OBJ_FLAG_SCROLLABLE);

    #define KB_MARGIN 20
    #define KB_GAP    4
    #define KB_TOP_H  60

    // Top bar: close button + text input
    lv_obj_t *btn_close = lv_btn_create(kb_overlay);
    lv_obj_set_size(btn_close, 40, 40);
    lv_obj_set_pos(btn_close, KB_MARGIN, 10);
    lv_obj_set_style_bg_color(btn_close, th_card(), 0);
    lv_obj_set_style_radius(btn_close, 8, 0);
    lv_obj_add_event_cb(btn_close, cb_kb_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xl = lv_label_create(btn_close);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, th_muted(), 0);
    lv_obj_center(xl);

    // Input display box
    lv_obj_t *input_box = lv_obj_create(kb_overlay);
    lv_obj_remove_style_all(input_box);
    lv_obj_set_size(input_box, SCREEN_W - KB_MARGIN * 2 - 52, 48);
    lv_obj_set_pos(input_box, KB_MARGIN + 52, 6);
    lv_obj_set_style_bg_color(input_box, th_card(), 0);
    lv_obj_set_style_bg_opa(input_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(input_box, 10, 0);
    lv_obj_set_style_border_width(input_box, 2, 0);
    lv_obj_set_style_border_color(input_box, th_border(), 0);
    lv_obj_clear_flag(input_box, LV_OBJ_FLAG_SCROLLABLE);

    kb_lbl_input = lv_label_create(input_box);
    lv_obj_set_width(kb_lbl_input, SCREEN_W - KB_MARGIN * 2 - 100);
    lv_label_set_long_mode(kb_lbl_input, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(kb_lbl_input, &font_swedish_14, 0);
    lv_obj_align(kb_lbl_input, LV_ALIGN_LEFT_MID, 12, 0);
    kb_update_display();

    // Build keyboard rows
    const char **rows;
    if (kb_mode == 2) rows = kb_rows_num;
    else if (kb_mode == 1) rows = kb_rows_upper;
    else rows = kb_rows_lower;

    int content_w = SCREEN_W - KB_MARGIN * 2;
    int y = KB_TOP_H;
    int row_idx = 0;
    int ki = 0;
    int key_h = (SCREEN_H - KB_TOP_H - KB_MARGIN) / 4 - KB_GAP;

    while (row_idx < 4) {
        int row_start = ki;
        int row_len = 0;
        while (rows[ki] != NULL) { ki++; row_len++; }
        ki++;  // skip NULL

        int total_gap = KB_GAP * (row_len - 1);
        int x = KB_MARGIN;

        for (int k = 0; k < row_len; k++) {
            const char *key = rows[row_start + k];
            int key_w;

            if (key[0] == ' ') {
                key_w = (content_w * 3) / (row_len + 3);
            } else if (key[0] == '\x01' || key[0] == '\x02' || key[0] == '\x03' ||
                       key[0] == '\x04' || key[0] == '\x05') {
                key_w = (content_w * 3) / (2 * (row_len + 3));
            } else {
                key_w = (content_w - total_gap) / row_len;
            }

            if (x + key_w > SCREEN_W - KB_MARGIN) {
                key_w = SCREEN_W - KB_MARGIN - x;
            }

            lv_obj_t *btn = lv_btn_create(kb_overlay);
            lv_obj_set_size(btn, key_w, key_h);
            lv_obj_set_pos(btn, x, y);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);

            if (key[0] == '\x04') {
                lv_obj_set_style_bg_color(btn, C_ACCENT, 0);
            } else if (key[0] == '\x01' || key[0] == '\x02') {
                lv_obj_set_style_bg_color(btn, th_border(), 0);
            } else {
                lv_obj_set_style_bg_color(btn, th_card(), 0);
            }

            lv_obj_add_event_cb(btn, cb_kb_key, LV_EVENT_CLICKED, (void *)key);

            lv_obj_t *lbl = lv_label_create(btn);
            if (key[0] == '\x01') lv_label_set_text(lbl, LV_SYMBOL_UP);
            else if (key[0] == '\x02') lv_label_set_text(lbl, LV_SYMBOL_BACKSPACE);
            else if (key[0] == '\x03') lv_label_set_text(lbl, "123");
            else if (key[0] == '\x04') lv_label_set_text(lbl, LV_SYMBOL_OK);
            else if (key[0] == '\x05') lv_label_set_text(lbl, "abc");
            else if (key[0] == ' ') lv_label_set_text(lbl, "_____");
            else lv_label_set_text(lbl, key);

            lv_obj_set_style_text_color(lbl, key[0] == '\x04' ? C_WHITE : th_fg(), 0);
            lv_obj_set_style_text_font(lbl, &font_swedish_14, 0);
            lv_obj_center(lbl);

            x += key_w + KB_GAP;
        }

        y += key_h + KB_GAP;
        row_idx++;
    }
}

static void hide_keyboard() {
    if (!kb_overlay) return;
    lv_obj_del(kb_overlay);
    kb_overlay = NULL;
    kb_lbl_input = NULL;
    ui_showing_keyboard = false;
}

// ══════════════════════════════════════
// REFRESH FUNCTIONS
// ══════════════════════════════════════

void do_refresh_all() {
    refresh_task();
    refresh_progress();
    refresh_dots();
    refresh_sidebar();
    refresh_clock();
}

static void set_refresh_state(bool refreshing) {
    ui_refreshing = refreshing;
    if (!btn_refresh_sb || !lbl_refresh_sb) return;

    if (refreshing) {
        lv_obj_add_state(btn_refresh_sb, LV_STATE_DISABLED);
        lv_label_set_text(lbl_refresh_sb, LV_SYMBOL_LOOP "  REFRESHING...");
        lv_obj_set_style_bg_color(btn_refresh_sb, th_card(), 0);
        lv_obj_set_style_text_color(lbl_refresh_sb, th_muted(), 0);
    } else {
        lv_obj_clear_state(btn_refresh_sb, LV_STATE_DISABLED);
        lv_label_set_text(lbl_refresh_sb, LV_SYMBOL_REFRESH "  REFRESH");
        lv_obj_set_style_bg_color(btn_refresh_sb, th_card(), 0);
        lv_obj_set_style_text_color(lbl_refresh_sb, th_fg(), 0);
    }
    lv_refr_now(NULL);
}

void trigger_task_refresh() {
    set_refresh_state(true);

    fetchTasks();

    ui_completed = 0;
    for (int i = 0; i < cal_task_count; i++) {
        if (cal_tasks[i].completed) ui_completed++;
    }
    if (ui_current >= cal_task_count) ui_current = 0;

    uiRebuild(settings_overlay != NULL);

    delay(500);

    set_refresh_state(false);
}

void refresh_clock() {
    if (!lbl_clock) return;
    struct tm tm;
    if (getLocalTime(&tm)) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(lbl_clock, buf);
    }
}

void refresh_task() {
    if (ui_list_view) return;
    if (cal_task_count <= 0) {
        lv_label_set_text(lbl_task_counter, "NO TASKS");
        lv_label_set_text(lbl_task_time, "");
        lv_label_set_text(lbl_task_title, "No tasks for today");
        lv_obj_set_style_text_color(lbl_task_title, th_muted(), 0);
        lv_obj_set_style_text_decor(lbl_task_title, LV_TEXT_DECOR_NONE, 0);
        lv_label_set_text(lbl_btn_complete, "Complete");
        lv_obj_set_style_bg_color(btn_complete, th_muted(), 0);
        lv_obj_add_flag(badge_completed, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    CalTask &t = cal_tasks[ui_current];

    char ctr[48];
    snprintf(ctr, sizeof(ctr), "TASK %d OF %d", ui_current + 1, cal_task_count);
    lv_label_set_text(lbl_task_counter, ctr);
    lv_label_set_text(lbl_task_time, t.time);
    lv_label_set_text(lbl_task_title, t.title);

    if (t.completed) {
        lv_obj_set_style_text_color(lbl_task_title, th_muted(), 0);
        lv_obj_set_style_text_decor(lbl_task_title, LV_TEXT_DECOR_STRIKETHROUGH, 0);
        lv_label_set_text(lbl_btn_complete, LV_SYMBOL_OK " Done");
        lv_obj_set_style_bg_color(btn_complete, th_muted(), 0);
        lv_obj_clear_flag(badge_completed, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_text_color(lbl_task_title, th_fg(), 0);
        lv_obj_set_style_text_decor(lbl_task_title, LV_TEXT_DECOR_NONE, 0);
        lv_label_set_text(lbl_btn_complete, "Complete");
        lv_obj_set_style_bg_color(btn_complete, C_ACCENT, 0);
        lv_obj_add_flag(badge_completed, LV_OBJ_FLAG_HIDDEN);
    }
}

void refresh_progress() {
    int pct = cal_task_count > 0 ? (ui_completed * 100) / cal_task_count : 0;
    lv_arc_set_value(arc_progress, pct);

    char n[4], t[8];
    snprintf(n, sizeof(n), "%d", ui_completed);
    snprintf(t, sizeof(t), "of %d", cal_task_count);
    lv_label_set_text(lbl_arc_num, n);
    lv_label_set_text(lbl_arc_total, t);
}

void refresh_dots() {
    if (ui_list_view) return;
    lv_obj_clean(dots_container);
    for (int i = 0; i < cal_task_count; i++) {
        lv_obj_t *d = lv_obj_create(dots_container);
        lv_obj_remove_style_all(d);
        lv_obj_set_height(d, 10);
        lv_obj_set_style_radius(d, 5, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        if (i == ui_current) {
            lv_obj_set_width(d, 28);
            lv_obj_set_style_bg_color(d, C_ACCENT, 0);
        } else if (cal_tasks[i].completed) {
            lv_obj_set_width(d, 10);
            lv_obj_set_style_bg_color(d, th_muted(), 0);
        } else {
            lv_obj_set_width(d, 10);
            lv_obj_set_style_bg_color(d, th_track(), 0);
        }
    }
}

void refresh_sidebar() {
    char s[8];
    snprintf(s, sizeof(s), "%d", streakStore.data.streak);
    lv_label_set_text(lbl_streak_num, s);

    const Level &lvl = streakStore.getLevel();
    lv_label_set_text(lbl_level_name, lvl.name);

    const Level *next = streakStore.getNextLevel();
    if (next) {
        char nx[32];
        snprintf(nx, sizeof(nx), "%dd to %s", next->threshold - streakStore.data.streak, next->name);
        lv_label_set_text(lbl_level_next, nx);
        lv_obj_clear_flag(lbl_level_next, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_level_next, LV_OBJ_FLAG_HIDDEN);
    }

    lv_bar_set_value(bar_xp, streakStore.getProgressToNext(), LV_ANIM_ON);

    char greet[20], date[32];
    getGreeting(greet, sizeof(greet));
    getDateStr(date, sizeof(date));
    lv_label_set_text(lbl_greeting, greet);
    lv_label_set_text(lbl_date, date);
}

// ══════════════════════════════════════
// COMPLETION SCREEN (full 800x480)
// ══════════════════════════════════════
void show_complete_screen() {
    if (complete_screen) return;
    ui_showing_complete = true;

    lv_obj_t *scr = lv_scr_act();
    complete_screen = lv_obj_create(scr);
    lv_obj_remove_style_all(complete_screen);
    lv_obj_set_size(complete_screen, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(complete_screen, 0, 0);
    lv_obj_set_style_bg_color(complete_screen, th_bg(), 0);
    lv_obj_set_style_bg_opa(complete_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(complete_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Checkmark circle
    lv_obj_t *circle = lv_obj_create(complete_screen);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 90, 90);
    lv_obj_align(circle, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(circle, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(circle, 45, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *check = lv_label_create(circle);
    lv_label_set_text(check, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(check, C_WHITE, 0);
    lv_obj_set_style_text_font(check, &font_swedish_48, 0);
    lv_obj_center(check);

    // Title
    lv_obj_t *title = lv_label_create(complete_screen);
    lv_label_set_text(title, "Day Complete");
    lv_obj_set_style_text_color(title, th_fg(), 0);
    lv_obj_set_style_text_font(title, &font_swedish_48, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 145);

    // Subtitle
    lv_obj_t *sub = lv_label_create(complete_screen);
    char buf[32];
    snprintf(buf, sizeof(buf), "All %d tasks done", cal_task_count);
    lv_label_set_text(sub, buf);
    lv_obj_set_style_text_color(sub, th_muted(), 0);
    lv_obj_set_style_text_font(sub, &font_swedish_14, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 200);

    // Streak info
    char streak_buf[48];
    snprintf(streak_buf, sizeof(streak_buf), "%d day streak  •  %s",
             streakStore.data.streak, streakStore.getLevel().name);
    lv_obj_t *streak_lbl = lv_label_create(complete_screen);
    lv_label_set_text(streak_lbl, streak_buf);
    lv_obj_set_style_text_color(streak_lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(streak_lbl, &font_swedish_28, 0);
    lv_obj_align(streak_lbl, LV_ALIGN_TOP_MID, 0, 260);

    // Back button
    lv_obj_t *btn = lv_btn_create(complete_screen);
    lv_obj_set_size(btn, 200, 48);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 340);
    lv_obj_set_style_bg_color(btn, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb_back_to_tasks, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back to tasks");
    lv_obj_set_style_text_color(bl, C_WHITE, 0);
    lv_obj_set_style_text_font(bl, &font_swedish_14, 0);
    lv_obj_center(bl);

    // Auto-dismiss after 5s
    lv_timer_create(cb_auto_dismiss, 5000, NULL);

    Serial.println("[UI] Completion screen shown!");
}

void hide_complete_screen() {
    if (!complete_screen) return;
    lv_obj_del(complete_screen);
    complete_screen = NULL;
    ui_showing_complete = false;
    Serial.println("[UI] Completion screen hidden");
}

// ══════════════════════════════════════
// SETTINGS OVERLAY
// ══════════════════════════════════════

void hide_settings_overlay() {
    if (!settings_overlay) return;
    lv_obj_del(settings_overlay);
    settings_overlay = NULL;
}

void uiRebuild(bool force_reopen_settings) {
    bool was_settings_open = (settings_overlay != NULL) || force_reopen_settings;
    lv_obj_clean(lv_scr_act());
    complete_screen = NULL;
    kb_overlay = NULL;
    kb_lbl_input = NULL;
    settings_overlay = NULL;
    buildTaskViewerUI();
    if (was_settings_open) {
        show_settings_overlay();
    }
}

void show_settings_overlay() {
    if (settings_overlay) return;

    settings_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(settings_overlay);
    lv_obj_set_size(settings_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(settings_overlay, 0, 0);
    lv_obj_set_style_bg_color(settings_overlay, th_bg(), 0);
    lv_obj_set_style_bg_opa(settings_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Close button
    lv_obj_t *btn_close = lv_btn_create(settings_overlay);
    lv_obj_set_size(btn_close, 40, 40);
    lv_obj_set_pos(btn_close, 20, 20);
    lv_obj_set_style_bg_color(btn_close, th_card(), 0);
    lv_obj_set_style_radius(btn_close, 8, 0);
    lv_obj_set_style_shadow_width(btn_close, 0, 0);
    lv_obj_add_event_cb(btn_close, [](lv_event_t *e) {
        hide_settings_overlay();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xl = lv_label_create(btn_close);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, th_muted(), 0);
    lv_obj_set_style_text_font(xl, &font_swedish_14, 0);
    lv_obj_center(xl);

    // Title
    lv_obj_t *title = lv_label_create(settings_overlay);
    lv_label_set_text(title, "Task Settings");
    lv_obj_set_style_text_color(title, th_fg(), 0);
    lv_obj_set_style_text_font(title, &font_swedish_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    // IP address (large)
    char ipBuf[32];
    WiFi.localIP().toString().toCharArray(ipBuf, sizeof(ipBuf));
    lv_obj_t *lbl_ip = lv_label_create(settings_overlay);
    lv_label_set_text(lbl_ip, ipBuf);
    lv_obj_set_style_text_color(lbl_ip, C_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_ip, &font_swedish_48, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, -40);

    // Instructions
    lv_obj_t *sub = lv_label_create(settings_overlay);
    lv_label_set_text(sub, "Open this address in your phone's browser\nto add or remove task providers.");
    lv_obj_set_style_text_color(sub, th_muted(), 0);
    lv_obj_set_style_text_font(sub, &font_swedish_14, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 30);

    // Dark mode button
    lv_obj_t *btn_dark = lv_btn_create(settings_overlay);
    lv_obj_set_size(btn_dark, 180, 44);
    lv_obj_set_style_bg_color(btn_dark, th_card(), 0);
    lv_obj_set_style_radius(btn_dark, 12, 0);
    lv_obj_set_style_border_width(btn_dark, 1, 0);
    lv_obj_set_style_border_color(btn_dark, th_border(), 0);
    lv_obj_set_style_shadow_width(btn_dark, 0, 0);
    lv_obj_add_event_cb(btn_dark, cb_toggle_dark, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn_dark, LV_ALIGN_BOTTOM_MID, -100, -60);

    lv_obj_t *dark_lbl = lv_label_create(btn_dark);
    lv_label_set_text(dark_lbl, ui_dark_mode ? LV_SYMBOL_EYE_CLOSE "  LIGHT" : LV_SYMBOL_EYE_OPEN "  DARK");
    lv_obj_set_style_text_color(dark_lbl, th_fg(), 0);
    lv_obj_set_style_text_font(dark_lbl, &font_swedish_14, 0);
    lv_obj_center(dark_lbl);

    // View mode button
    lv_obj_t *btn_view = lv_btn_create(settings_overlay);
    lv_obj_set_size(btn_view, 180, 44);
    lv_obj_set_style_bg_color(btn_view, th_card(), 0);
    lv_obj_set_style_radius(btn_view, 12, 0);
    lv_obj_set_style_border_width(btn_view, 1, 0);
    lv_obj_set_style_border_color(btn_view, th_border(), 0);
    lv_obj_set_style_shadow_width(btn_view, 0, 0);
    lv_obj_add_event_cb(btn_view, cb_toggle_view, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn_view, LV_ALIGN_BOTTOM_MID, 100, -60);

    lv_obj_t *view_lbl = lv_label_create(btn_view);
    lv_label_set_text(view_lbl, ui_list_view ? LV_SYMBOL_LIST "  LIST VIEW" : LV_SYMBOL_FILE "  SINGLE VIEW");
    lv_obj_set_style_text_color(view_lbl, th_fg(), 0);
    lv_obj_set_style_text_font(view_lbl, &font_swedish_14, 0);
    lv_obj_center(view_lbl);
}

// ══════════════════════════════════════
// BUILD MAIN UI
// ══════════════════════════════════════
void buildTaskViewerUI() {
    static bool ui_init = false;
    if (!ui_init) {
        ui_dark_mode = taskStoreGetDarkMode();
        ui_list_view = taskStoreGetListView();
        ui_init = true;
    }
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, th_bg(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ═══ LEFT SIDEBAR (240px) ═══
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_remove_style_all(sb);
    lv_obj_set_size(sb, SIDEBAR_W, SCREEN_H);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, th_sidebar(), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 1, 0);
    lv_obj_set_style_border_color(sb, th_border(), 0);
    lv_obj_set_style_border_side(sb, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    // Greeting + Date
    lbl_greeting = lv_label_create(sb);
    lv_label_set_text(lbl_greeting, "GOOD MORNING");
    lv_obj_set_style_text_color(lbl_greeting, th_muted(), 0);
    lv_obj_set_style_text_font(lbl_greeting, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_greeting, 16, 16);

    lbl_date = lv_label_create(sb);
    lv_label_set_text(lbl_date, "Loading...");
    lv_obj_set_style_text_color(lbl_date, th_fg(), 0);
    lv_obj_set_style_text_font(lbl_date, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_date, 16, 34);

    // Streak card
    lv_obj_t *sc = lv_obj_create(sb);
    lv_obj_remove_style_all(sc);
    lv_obj_set_size(sc, SIDEBAR_W - 32, 60);
    lv_obj_set_pos(sc, 16, 64);
    lv_obj_set_style_bg_color(sc, th_card(), 0);
    lv_obj_set_style_bg_opa(sc, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sc, 12, 0);
    lv_obj_clear_flag(sc, LV_OBJ_FLAG_SCROLLABLE);

    // Orange accent square
    lv_obj_t *flame = lv_obj_create(sc);
    lv_obj_remove_style_all(flame);
    lv_obj_set_size(flame, 36, 36);
    lv_obj_set_pos(flame, 10, 12);
    lv_obj_set_style_bg_color(flame, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(flame, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(flame, 8, 0);
    lv_obj_clear_flag(flame, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *flame_sym = lv_label_create(flame);
    lv_label_set_text(flame_sym, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(flame_sym, C_WHITE, 0);
    lv_obj_set_style_text_font(flame_sym, &font_swedish_28, 0);
    lv_obj_center(flame_sym);

    lbl_streak_num = lv_label_create(sc);
    lv_label_set_text(lbl_streak_num, "0");
    lv_obj_set_style_text_color(lbl_streak_num, th_fg(), 0);
    lv_obj_set_style_text_font(lbl_streak_num, &font_swedish_28, 0);
    lv_obj_set_pos(lbl_streak_num, 56, 6);

    lbl_streak_label = lv_label_create(sc);
    lv_label_set_text(lbl_streak_label, "day streak");
    lv_obj_set_style_text_color(lbl_streak_label, th_muted(), 0);
    lv_obj_set_style_text_font(lbl_streak_label, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_streak_label, 56, 36);

    // Level card
    lv_obj_t *lc = lv_obj_create(sb);
    lv_obj_remove_style_all(lc);
    lv_obj_set_size(lc, SIDEBAR_W - 32, 56);
    lv_obj_set_pos(lc, 16, 132);
    lv_obj_set_style_bg_color(lc, th_card(), 0);
    lv_obj_set_style_bg_opa(lc, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lc, 12, 0);
    lv_obj_clear_flag(lc, LV_OBJ_FLAG_SCROLLABLE);

    lbl_level_name = lv_label_create(lc);
    lv_label_set_text(lbl_level_name, "Starter");
    lv_obj_set_style_text_color(lbl_level_name, th_fg(), 0);
    lv_obj_set_style_text_font(lbl_level_name, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_level_name, 14, 6);

    lbl_level_next = lv_label_create(lc);
    lv_label_set_text(lbl_level_next, "");
    lv_obj_set_style_text_color(lbl_level_next, th_muted(), 0);
    lv_obj_set_style_text_font(lbl_level_next, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_level_next, 14, 24);
    lv_obj_set_width(lbl_level_next, SIDEBAR_W - 64);
    lv_label_set_long_mode(lbl_level_next, LV_LABEL_LONG_DOT);

    bar_xp = lv_bar_create(lc);
    lv_obj_set_size(bar_xp, SIDEBAR_W - 64, 6);
    lv_obj_set_pos(bar_xp, 14, 44);
    lv_obj_set_style_bg_color(bar_xp, th_track(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_xp, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_xp, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_xp, 3, LV_PART_INDICATOR);

    // Progress ring
    arc_progress = lv_arc_create(sb);
    lv_obj_set_size(arc_progress, 110, 110);
    lv_obj_set_pos(arc_progress, (SIDEBAR_W - 110) / 2, 196);
    lv_arc_set_rotation(arc_progress, 270);
    lv_arc_set_bg_angles(arc_progress, 0, 360);
    lv_arc_set_range(arc_progress, 0, 100);
    lv_arc_set_value(arc_progress, 0);
    lv_obj_remove_style(arc_progress, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_progress, th_track(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_progress, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_progress, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_progress, true, LV_PART_INDICATOR);

    lbl_arc_num = lv_label_create(sb);
    lv_label_set_text(lbl_arc_num, "0");
    lv_obj_set_style_text_color(lbl_arc_num, th_fg(), 0);
    lv_obj_set_style_text_font(lbl_arc_num, &font_swedish_28, 0);
    lv_obj_set_pos(lbl_arc_num, (SIDEBAR_W / 2) - 8, 230);

    lbl_arc_total = lv_label_create(sb);
    lv_label_set_text(lbl_arc_total, "of 0");
    lv_obj_set_style_text_color(lbl_arc_total, th_muted(), 0);
    lv_obj_set_style_text_font(lbl_arc_total, &font_swedish_14, 0);
    lv_obj_set_pos(lbl_arc_total, (SIDEBAR_W / 2) - 14, 260);

    // ── Bottom toolbar ──
    // Refresh button
    btn_refresh_sb = lv_btn_create(sb);
    lv_obj_set_size(btn_refresh_sb, SIDEBAR_W - 32, 40);
    lv_obj_set_pos(btn_refresh_sb, 16, SCREEN_H - 48);
    lv_obj_set_style_bg_color(btn_refresh_sb, th_card(), 0);
    lv_obj_set_style_bg_color(btn_refresh_sb, th_border(), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_refresh_sb, 12, 0);
    lv_obj_set_style_shadow_width(btn_refresh_sb, 0, 0);
    lv_obj_add_event_cb(btn_refresh_sb, [](lv_event_t *e) {
        Serial.println("[UI] Refresh tapped!");
        trigger_task_refresh();
    }, LV_EVENT_CLICKED, NULL);

    lbl_refresh_sb = lv_label_create(btn_refresh_sb);
    lv_label_set_text(lbl_refresh_sb, ui_refreshing ? LV_SYMBOL_LOOP "  REFRESHING..." : LV_SYMBOL_REFRESH "  REFRESH");
    lv_obj_set_style_text_color(lbl_refresh_sb, ui_refreshing ? th_muted() : th_fg(), 0);
    lv_obj_set_style_text_font(lbl_refresh_sb, &font_swedish_14, 0);
    lv_obj_center(lbl_refresh_sb);

    if (ui_refreshing) {
        lv_obj_add_state(btn_refresh_sb, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn_refresh_sb, th_card(), 0);
    }

    // ═══ RIGHT PANEL ═══
    lv_obj_t *mp = lv_obj_create(scr);
    lv_obj_remove_style_all(mp);
    lv_obj_set_size(mp, MAIN_W, SCREEN_H);
    lv_obj_set_pos(mp, SIDEBAR_W, 0);
    lv_obj_set_style_bg_color(mp, th_bg(), 0);
    lv_obj_set_style_bg_opa(mp, LV_OPA_COVER, 0);
    lv_obj_clear_flag(mp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mp, cb_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(mp, LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_mp = mp;

    // Clock display
    lbl_clock = lv_label_create(mp);
    lv_label_set_text(lbl_clock, "00:00");
    lv_obj_set_style_text_color(lbl_clock, th_fg(), 0);
    lv_obj_set_style_text_font(lbl_clock, &font_swedish_28, 0);
    lv_obj_set_pos(lbl_clock, 30, 16);

    // WiFi status icon (moved left to make room for settings button)
    lv_obj_t *wifi_icon = lv_label_create(mp);
    lv_label_set_text(wifi_icon, WiFi.isConnected() ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(wifi_icon, WiFi.isConnected() ? th_muted() : C_ACCENT, 0);
    lv_obj_set_style_text_font(wifi_icon, &font_swedish_14, 0);
    lv_obj_set_pos(wifi_icon, MAIN_W - 185, 22);

    // Settings button
    lv_obj_t *btn_settings = lv_btn_create(mp);
    lv_obj_set_size(btn_settings, 40, 40);
    lv_obj_set_pos(btn_settings, MAIN_W - 160, 12);
    lv_obj_set_style_bg_color(btn_settings, th_card(), 0);
    lv_obj_set_style_radius(btn_settings, 10, 0);
    lv_obj_set_style_border_width(btn_settings, 1, 0);
    lv_obj_set_style_border_color(btn_settings, th_border(), 0);
    lv_obj_set_style_shadow_width(btn_settings, 0, 0);
    lv_obj_add_event_cb(btn_settings, [](lv_event_t *e) {
        show_settings_overlay();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stl = lv_label_create(btn_settings);
    lv_label_set_text(stl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(stl, th_muted(), 0);
    lv_obj_set_style_text_font(stl, &font_swedish_14, 0);
    lv_obj_align(stl, LV_ALIGN_CENTER, 0, -3);

    // Sleep button (next to WiFi)
    lv_obj_t *btn_sleep = lv_btn_create(mp);
    lv_obj_set_size(btn_sleep, 40, 40);
    lv_obj_set_pos(btn_sleep, MAIN_W - 110, 12);
    lv_obj_set_style_bg_color(btn_sleep, th_card(), 0);
    lv_obj_set_style_radius(btn_sleep, 10, 0);
    lv_obj_set_style_border_width(btn_sleep, 1, 0);
    lv_obj_set_style_border_color(btn_sleep, th_border(), 0);
    lv_obj_set_style_shadow_width(btn_sleep, 0, 0);
    lv_obj_add_event_cb(btn_sleep, [](lv_event_t *e) {
        setDisplaySleep(true);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sl = lv_label_create(btn_sleep);
    lv_label_set_text(sl, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(sl, th_muted(), 0);
    lv_obj_set_style_text_font(sl, &font_swedish_14, 0);
    lv_obj_align(sl, LV_ALIGN_CENTER, 0, -3);

    // Add task button
    lv_obj_t *btn_add = lv_btn_create(mp);
    lv_obj_set_size(btn_add, 40, 40);
    lv_obj_set_pos(btn_add, MAIN_W - 60, 12);
    lv_obj_set_style_bg_color(btn_add, th_card(), 0);
    lv_obj_set_style_radius(btn_add, 10, 0);
    lv_obj_set_style_border_width(btn_add, 1, 0);
    lv_obj_set_style_border_color(btn_add, th_border(), 0);
    lv_obj_add_event_cb(btn_add, cb_add_task, LV_EVENT_CLICKED, NULL);

    lv_obj_t *al = lv_label_create(btn_add);
    lv_label_set_text(al, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(al, th_muted(), 0);
    lv_obj_set_style_text_font(al, &font_swedish_14, 0);
    lv_obj_align(al, LV_ALIGN_CENTER, 0, -3);

    if (ui_list_view) {
        // List View
        lv_obj_t *list_container = lv_obj_create(mp);
        lv_obj_remove_style_all(list_container);
        lv_obj_set_size(list_container, MAIN_W - 60, 380);
        lv_obj_set_pos(list_container, 30, 80);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_container, 10, 0);
        lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);

        if (cal_task_count <= 0) {
            lv_obj_t *lbl_empty = lv_label_create(list_container);
            lv_label_set_text(lbl_empty, "No tasks for today");
            lv_obj_set_style_text_color(lbl_empty, th_muted(), 0);
            lv_obj_set_style_text_font(lbl_empty, &font_swedish_14, 0);
            lv_obj_set_style_text_align(lbl_empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl_empty, MAIN_W - 60);
            lv_obj_align(lbl_empty, LV_ALIGN_CENTER, 0, 0);
        }

        for (int i = 0; i < cal_task_count; i++) {
            lv_obj_t *card = lv_obj_create(list_container);
            lv_obj_remove_style_all(card);
            lv_obj_set_size(card, MAIN_W - 60, 56);
            lv_obj_set_style_bg_color(card, th_card(), 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(card, 12, 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_color(card, th_border(), 0);
            lv_obj_set_style_pad_left(card, 16, 0);
            lv_obj_set_style_pad_right(card, 16, 0);
            lv_obj_set_style_pad_column(card, 16, 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            // Checkmark button (interactive)
            lv_obj_t *btn_check = lv_btn_create(card);
            lv_obj_set_size(btn_check, 24, 24);
            lv_obj_set_style_radius(btn_check, 12, 0); // Round circle
            lv_obj_set_style_bg_color(btn_check, cal_tasks[i].completed ? C_ACCENT : lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(btn_check, cal_tasks[i].completed ? LV_OPA_COVER : LV_OPA_0, 0);
            lv_obj_set_style_border_width(btn_check, 2, 0);
            lv_obj_set_style_border_color(btn_check, cal_tasks[i].completed ? C_ACCENT : th_muted(), 0);
            lv_obj_set_style_shadow_width(btn_check, 0, 0);
            lv_obj_add_event_cb(btn_check, cb_list_toggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);

            if (cal_tasks[i].completed) {
                lv_obj_t *check_sym = lv_label_create(btn_check);
                lv_label_set_text(check_sym, LV_SYMBOL_OK);
                lv_obj_set_style_text_color(check_sym, C_WHITE, 0);
                lv_obj_set_style_text_font(check_sym, &font_swedish_14, 0);
                lv_obj_center(check_sym);
            }

            // Task title
            lv_obj_t *lbl_title = lv_label_create(card);
            lv_label_set_text(lbl_title, cal_tasks[i].title);
            lv_obj_set_style_text_color(lbl_title, cal_tasks[i].completed ? th_muted() : th_fg(), 0);
            if (cal_tasks[i].completed) {
                lv_obj_set_style_text_decor(lbl_title, LV_TEXT_DECOR_STRIKETHROUGH, 0);
            }
            lv_obj_set_style_text_font(lbl_title, &font_swedish_14, 0);
            lv_obj_set_width(lbl_title, MAIN_W - 200);
            lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
            lv_obj_add_event_cb(lbl_title, cb_list_toggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_CLICKABLE);

            // Task Time
            if (cal_tasks[i].time[0] != '\0') {
                lv_obj_t *lbl_time = lv_label_create(card);
                lv_label_set_text(lbl_time, cal_tasks[i].time);
                lv_obj_set_style_text_color(lbl_time, th_muted(), 0);
                lv_obj_set_style_text_font(lbl_time, &font_swedish_14, 0);
                lv_obj_set_flex_grow(lbl_title, 1);
            } else {
                lv_obj_set_flex_grow(lbl_title, 1);
            }
        }

        refresh_progress();
        refresh_sidebar();
        refresh_clock();
    } else {
        // Hero View Layout
        // Task counter + time
        lbl_task_counter = lv_label_create(mp);
        lv_label_set_text(lbl_task_counter, "TASK 1 OF 1");
        lv_obj_set_style_text_color(lbl_task_counter, C_ACCENT, 0);
        lv_obj_set_style_text_font(lbl_task_counter, &font_swedish_14, 0);
        lv_obj_set_pos(lbl_task_counter, 30, 120);

        lbl_task_time = lv_label_create(mp);
        lv_label_set_text(lbl_task_time, "");
        lv_obj_set_style_text_color(lbl_task_time, th_muted(), 0);
        lv_obj_set_style_text_font(lbl_task_time, &font_swedish_14, 0);
        lv_obj_set_pos(lbl_task_time, 170, 120);

        // Completed badge
        badge_completed = lv_obj_create(mp);
        lv_obj_remove_style_all(badge_completed);
        lv_obj_set_size(badge_completed, 120, 28);
        lv_obj_set_pos(badge_completed, MAIN_W - 160, 117);
        lv_obj_set_style_bg_color(badge_completed, C_ACCENT, 0);
        lv_obj_set_style_bg_opa(badge_completed, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge_completed, 14, 0);
        lv_obj_t *bl = lv_label_create(badge_completed);
        lv_label_set_text(bl, "COMPLETED");
        lv_obj_set_style_text_color(bl, C_WHITE, 0);
        lv_obj_set_style_text_font(bl, &font_swedish_14, 0);
        lv_obj_center(bl);
        lv_obj_add_flag(badge_completed, LV_OBJ_FLAG_HIDDEN);

        // Hero task title
        lbl_task_title = lv_label_create(mp);
        lv_label_set_text(lbl_task_title, "Loading...");
        lv_obj_set_style_text_color(lbl_task_title, th_fg(), 0);
        lv_obj_set_style_text_font(lbl_task_title, &font_swedish_48, 0);
        lv_obj_set_width(lbl_task_title, MAIN_W - 70);
        lv_label_set_long_mode(lbl_task_title, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(lbl_task_title, 30, 155);

        // Bottom bar
        dots_container = lv_obj_create(mp);
        lv_obj_remove_style_all(dots_container);
        lv_obj_set_size(dots_container, 300, 14);
        lv_obj_set_pos(dots_container, 30, SCREEN_H - 50);
        lv_obj_set_flex_flow(dots_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(dots_container, 6, 0);
        lv_obj_set_flex_align(dots_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        btn_complete = lv_btn_create(mp);
        lv_obj_set_size(btn_complete, 140, 48);
        lv_obj_set_pos(btn_complete, MAIN_W - 210, SCREEN_H - 64);
        lv_obj_set_style_bg_color(btn_complete, C_ACCENT, 0);
        lv_obj_set_style_bg_opa(btn_complete, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_complete, 12, 0);
        lv_obj_set_style_shadow_width(btn_complete, 12, 0);
        lv_obj_set_style_shadow_color(btn_complete, C_ACCENT, 0);
        lv_obj_set_style_shadow_opa(btn_complete, LV_OPA_30, 0);
        lv_obj_add_event_cb(btn_complete, cb_complete, LV_EVENT_CLICKED, NULL);

        lbl_btn_complete = lv_label_create(btn_complete);
        lv_label_set_text(lbl_btn_complete, "Complete");
        lv_obj_set_style_text_color(lbl_btn_complete, C_WHITE, 0);
        lv_obj_set_style_text_font(lbl_btn_complete, &font_swedish_14, 0);
        lv_obj_center(lbl_btn_complete);

        lv_obj_t *btn_next = lv_btn_create(mp);
        lv_obj_set_size(btn_next, 48, 48);
        lv_obj_set_pos(btn_next, MAIN_W - 62, SCREEN_H - 64);
        lv_obj_set_style_bg_color(btn_next, th_fg(), 0);
        lv_obj_set_style_bg_opa(btn_next, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_next, 12, 0);
        lv_obj_add_event_cb(btn_next, cb_next, LV_EVENT_CLICKED, NULL);

        lv_obj_t *nl = lv_label_create(btn_next);
        lv_label_set_text(nl, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(nl, th_bg(), 0);
        lv_obj_center(nl);

        refresh_dots();
        refresh_progress();
        refresh_task();
        refresh_sidebar();
        refresh_clock();
    }

    Serial.println("[UI] Task Viewer built (800x480)");
}

#endif /* UI_TASKVIEWER_H */
