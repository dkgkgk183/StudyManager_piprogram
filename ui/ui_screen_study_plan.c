#include "ui_screen_study_plan.h"
#include <stdio.h>

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

void ui_screen_study_plan_create(const char *device_number) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "공부 계획 화면");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &font_korean_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    char buf[64];
    snprintf(buf, sizeof(buf), "기기: %s", device_number);
    lv_obj_t *dev_label = lv_label_create(scr);
    lv_label_set_text(dev_label, buf);
    lv_obj_set_style_text_color(dev_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(dev_label, &font_korean_16, 0);
    lv_obj_align(dev_label, LV_ALIGN_CENTER, 0, 15);

    lv_screen_load(scr);
}
