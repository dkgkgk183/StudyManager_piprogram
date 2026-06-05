#include "ui_screen_nfc_test.h"
#include "../app/app_nfc.h"
#include <stdio.h>

LV_FONT_DECLARE(font_korean_20);
LV_FONT_DECLARE(lv_font_montserrat_32);

static lv_obj_t *status_circle;
static lv_obj_t *status_icon;
static lv_obj_t *status_label;
static lv_obj_t *uid_label;
static lv_obj_t *conn_dot;
static lv_obj_t *conn_label;

static void nfc_update_cb(lv_timer_t *t) {
    (void)t;

    if (g_nfc.connected) {
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0x008833), 0);
        lv_label_set_text(conn_label, "PN532 연결됨");
        lv_obj_set_style_text_color(conn_label, lv_color_hex(0x2E7D32), 0);
    } else {
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0xCC0000), 0);
        lv_label_set_text(conn_label, "PN532 연결 안됨");
        lv_obj_set_style_text_color(conn_label, lv_color_hex(0xCC0000), 0);
    }

    if (g_nfc.tag_detected) {
        lv_obj_set_style_bg_color(status_circle, lv_color_hex(0x1DB954), 0);
        lv_label_set_text(status_icon, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(status_icon, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(status_label, "태그 감지됨!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x1DB954), 0);
        char buf[64];
        snprintf(buf, sizeof(buf), "UID: %s", g_nfc.uid_str);
        lv_label_set_text(uid_label, buf);
        lv_obj_set_style_text_color(uid_label, lv_color_hex(0x222222), 0);
    } else {
        lv_obj_set_style_bg_color(status_circle, lv_color_hex(0xCCCCCC), 0);
        lv_label_set_text(status_icon, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(status_icon, lv_color_hex(0x666666), 0);
        lv_label_set_text(status_label, "태그 없음");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x666666), 0);
        lv_label_set_text(uid_label, "스마트폰을 센서 위에 대주세요");
        lv_obj_set_style_text_color(uid_label, lv_color_hex(0x9999AA), 0);
    }
}

void ui_screen_nfc_test_create(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // 상단 연결 상태 바
    lv_obj_t *conn_bar = lv_obj_create(scr);
    lv_obj_set_size(conn_bar, 240, 36);
    lv_obj_set_style_bg_color(conn_bar, lv_color_hex(0xF2F2F2), 0);
    lv_obj_set_style_border_width(conn_bar, 0, 0);
    lv_obj_set_style_radius(conn_bar, 0, 0);
    lv_obj_align(conn_bar, LV_ALIGN_TOP_MID, 0, 0);

    conn_dot = lv_obj_create(conn_bar);
    lv_obj_set_size(conn_dot, 12, 12);
    lv_obj_set_style_radius(conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0xCC0000), 0);
    lv_obj_set_style_border_width(conn_dot, 0, 0);
    lv_obj_align(conn_dot, LV_ALIGN_LEFT_MID, 14, 0);

    conn_label = lv_label_create(conn_bar);
    lv_label_set_text(conn_label, "PN532 확인 중...");
    lv_obj_set_style_text_color(conn_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(conn_label, &font_korean_20, 0);
    lv_obj_align(conn_label, LV_ALIGN_LEFT_MID, 36, 0);

    // 타이틀
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "NFC 인식 테스트");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &font_korean_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    // 중앙 원형 상태
    status_circle = lv_obj_create(scr);
    lv_obj_set_size(status_circle, 140, 140);
    lv_obj_set_style_radius(status_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_circle, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(status_circle, 0, 0);
    lv_obj_align(status_circle, LV_ALIGN_CENTER, 0, -30);

    status_icon = lv_label_create(status_circle);
    lv_label_set_text(status_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(status_icon, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(status_icon, &lv_font_montserrat_32, 0);
    lv_obj_center(status_icon);

    // 상태 텍스트
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "태그 없음");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(status_label, &font_korean_20, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 70);

    // UID 텍스트
    uid_label = lv_label_create(scr);
    lv_label_set_text(uid_label, "스마트폰을 센서 위에 대주세요");
    lv_obj_set_style_text_color(uid_label, lv_color_hex(0x9999AA), 0);
    lv_obj_set_style_text_font(uid_label, &font_korean_20, 0);
    lv_obj_set_width(uid_label, 220);
    lv_obj_set_style_text_align(uid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(uid_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(uid_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_timer_create(nfc_update_cb, 200, NULL);
}
