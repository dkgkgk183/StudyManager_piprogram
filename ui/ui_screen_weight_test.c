#include "ui_screen_weight_test.h"
#include "app/app_hx711.h"
#include <stdio.h>

LV_FONT_DECLARE(font_korean_20);

static lv_obj_t *weight_label;
static lv_obj_t *raw_label;

static void timer_cb(lv_timer_t *timer) {
    if (!hx711_is_ready()) return;

    int32_t raw = hx711_read_raw();
    float weight = hx711_read_weight();
    if (weight < 0) weight = 0;

    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f g", weight);
    lv_label_set_text(weight_label, buf);

    snprintf(buf, sizeof(buf), "raw: %ld", (long)raw);
    lv_label_set_text(raw_label, buf);
}

void ui_screen_weight_test_create(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Weight Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    weight_label = lv_label_create(scr);
    lv_label_set_text(weight_label, "0.0 g");
    lv_obj_set_style_text_color(weight_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(weight_label, &font_korean_20, 0);
    lv_obj_align(weight_label, LV_ALIGN_CENTER, 0, -10);

    raw_label = lv_label_create(scr);
    lv_label_set_text(raw_label, "raw: 0");
    lv_obj_set_style_text_color(raw_label, lv_color_hex(0x999999), 0);
    lv_obj_align(raw_label, LV_ALIGN_CENTER, 0, 20);

    lv_timer_create(timer_cb, 200, NULL);
}
