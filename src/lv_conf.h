/**
 * LVGL Configuration for Elecrow Wizee 5.0" (800x480)
 * Minimal config — only what we need
 */

#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Memory */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U)

/* Display */
#define LV_HOR_RES_MAX 800
#define LV_VER_RES_MAX 480
#define LV_DPI_DEF 130

/* Features */
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1

/* Built-in fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Text */
#define LV_USE_LABEL 1
#define LV_LABEL_TEXT_SELECTION 0

/* Widgets */
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 0
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMG 1
#define LV_USE_LINE 0
#define LV_USE_ROLLER 0
#define LV_USE_SLIDER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_ARC 1
#define LV_USE_SPINNER 0
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_MENU 0

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/* Animation */
#define LV_USE_ANIM 1

/* Themes */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0

#endif /* LV_CONF_H */

#endif /* End of "Content enable" */
