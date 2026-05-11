#include "ui_tab_camera.h"

LV_FONT_DECLARE(font_korean_16);

void ui_tab_camera_create(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x9999AA), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "카메라\n준비 중...");
    lv_obj_set_style_text_color(label, lv_color_hex(0x777788), 0);
    lv_obj_set_style_text_font(label, &font_korean_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);
}