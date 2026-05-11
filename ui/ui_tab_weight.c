#include "ui_tab_weight.h"
#include "app/app_hx711.h"
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(font_korean_16);
extern lv_font_t lv_font_montserrat_32;

#define WEIGHT_MAX 500  // 최대 표시 무게 (500g)

static lv_obj_t *weight_label = NULL;
static lv_obj_t *bar = NULL;
static lv_timer_t *hx711_timer = NULL;

extern lv_font_t lv_font_montserrat_32;

static void hx711_timer_cb(lv_timer_t *timer) {
    (void)timer;

    if (!hx711_is_ready()) {
        lv_label_set_text(weight_label, "대기 중...");
        lv_bar_set_value(bar, 0, LV_ANIM_ON);
        return;
    }

    float weight = hx711_read_weight();
    if (weight < 0) weight = 0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fg", weight);
    lv_label_set_text(weight_label, buf);

    int bar_val = (int)((weight / WEIGHT_MAX) * 100);
    if (bar_val > 100) bar_val = 100;
    lv_bar_set_value(bar, bar_val, LV_ANIM_ON);
}

void ui_tab_weight_create(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);

    // 제목
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "무게 센서");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(title, &font_korean_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 게이지 (바)
    bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 200, 20);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, -20);

    // 무게 표시 라벨 (큰 텍스트)
    weight_label = lv_label_create(parent);
    lv_label_set_text(weight_label, "0.0g");
    lv_obj_set_style_text_color(weight_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(weight_label, &lv_font_montserrat_32, 0);
    lv_obj_align(weight_label, LV_ALIGN_CENTER, 0, 15);

    // 500ms 간격으로 값 업데이트
    hx711_timer = lv_timer_create(hx711_timer_cb, 500, NULL);
}
