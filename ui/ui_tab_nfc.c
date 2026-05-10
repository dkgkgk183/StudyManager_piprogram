#include "ui_tab_nfc.h"
#include "../app/app_nfc.h"
#include <stdio.h>

static lv_obj_t *status_circle;
static lv_obj_t *status_icon;
static lv_obj_t *status_label;
static lv_obj_t *uid_label;
static lv_obj_t *conn_dot;
static lv_obj_t *conn_label;

static void nfc_update_cb(lv_timer_t *t) {
    (void)t;

    // 연결 상태
    if (g_nfc.connected) {
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0x00FF88), 0);
        lv_label_set_text(conn_label, "PN532 연결됨");
    } else {
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0xFF3333), 0);
        lv_label_set_text(conn_label, "PN532 연결 안됨");
    }

    // 태그 감지 상태
    if (g_nfc.tag_detected) {
        lv_obj_set_style_bg_color(status_circle, lv_color_hex(0x1DB954), 0);
        lv_label_set_text(status_icon, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(status_icon, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(status_label, "태그 감지됨!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x1DB954), 0);
        char buf[48];
        snprintf(buf, sizeof(buf), "UID: %s", g_nfc.uid_str);
        lv_label_set_text(uid_label, buf);
        lv_obj_set_style_text_color(uid_label, lv_color_hex(0xAAAAAA), 0);
    } else {
        lv_obj_set_style_bg_color(status_circle, lv_color_hex(0x2A2A2A), 0);
        lv_label_set_text(status_icon, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(status_icon, lv_color_hex(0x555555), 0);
        lv_label_set_text(status_label, "태그 없음");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x888888), 0);
        lv_label_set_text(uid_label, "스마트폰을 센서 위에 대주세요");
        lv_obj_set_style_text_color(uid_label, lv_color_hex(0x444466), 0);
    }
}

void ui_tab_nfc_create(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0D0D1A), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // 상단 연결 상태 바
    lv_obj_t *conn_bar = lv_obj_create(parent);
    lv_obj_set_size(conn_bar, 240, 30);
    lv_obj_set_style_bg_color(conn_bar, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(conn_bar, 0, 0);
    lv_obj_set_style_radius(conn_bar, 0, 0);
    lv_obj_align(conn_bar, LV_ALIGN_TOP_MID, 0, 0);

    conn_dot = lv_obj_create(conn_bar);
    lv_obj_set_size(conn_dot, 10, 10);
    lv_obj_set_style_radius(conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_border_width(conn_dot, 0, 0);
    lv_obj_align(conn_dot, LV_ALIGN_LEFT_MID, 12, 0);

    conn_label = lv_label_create(conn_bar);
    lv_label_set_text(conn_label, "PN532 확인 중...");
    lv_obj_set_style_text_color(conn_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(conn_label, LV_ALIGN_LEFT_MID, 30, 0);

    // 중앙 원형 상태
    status_circle = lv_obj_create(parent);
    lv_obj_set_size(status_circle, 100, 100);
    lv_obj_set_style_radius(status_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_circle, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_width(status_circle, 0, 0);
    lv_obj_align(status_circle, LV_ALIGN_TOP_MID, 0, 45);

    status_icon = lv_label_create(status_circle);
    lv_label_set_text(status_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(status_icon, lv_color_hex(0x555555), 0);
    lv_obj_center(status_icon);

    // 상태 텍스트
    status_label = lv_label_create(parent);
    lv_label_set_text(status_label, "태그 없음");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x888888), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 158);

    // 구분선
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, 180, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x222244), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 178);

    // UID 텍스트
    uid_label = lv_label_create(parent);
    lv_label_set_text(uid_label, "스마트폰을 센서 위에 대주세요");
    lv_obj_set_style_text_color(uid_label, lv_color_hex(0x444466), 0);
    lv_obj_set_width(uid_label, 220);
    lv_obj_set_style_text_align(uid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(uid_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(uid_label, LV_ALIGN_TOP_MID, 0, 188);

    lv_timer_create(nfc_update_cb, 300, NULL);
}