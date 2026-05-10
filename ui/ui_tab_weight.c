#include "ui_tab_weight.h"

LV_FONT_DECLARE(font_korean_16);

void ui_tab_weight_create(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0D0D1A), 0);

    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x333355), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "무게 센서\n준비 중...");
    lv_obj_set_style_text_color(label, lv_color_hex(0x555577), 0);
    lv_obj_set_style_text_font(label, &font_korean_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);
}