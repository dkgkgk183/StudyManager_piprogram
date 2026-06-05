#include "ui_screen_device_select.h"
#include <stdio.h>
#include <string.h>
#include "app/app_supabase.h"
#include "ui_screen_intro.h"

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

static char target_nfc_id[32];
static lv_obj_t *result_label;
static lv_obj_t *device_scr;  // 기기 선택 화면 참거 (intro 복귀용)

static void btn_event_cb(lv_event_t *e) {
    const char *device_number = lv_event_get_user_data(e);

    lv_label_set_text(result_label, "등록 중...");
    lv_obj_set_style_text_color(result_label, lv_color_hex(0x666666), 0);
    lv_obj_align(result_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    bool ok = supabase_register_device(device_number, target_nfc_id);
    if (ok) {
        supabase_set_user_id(device_number);
        lv_label_set_text(result_label, "등록 완료!");
        lv_obj_set_style_text_color(result_label, lv_color_hex(0x2E7D32), 0);
    } else {
        lv_label_set_text(result_label, "등록 실패");
        lv_obj_set_style_text_color(result_label, lv_color_hex(0xC62828), 0);
    }
    lv_obj_align(result_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void ui_screen_device_select_create(const char *nfc_id) {
    strncpy(target_nfc_id, nfc_id, sizeof(target_nfc_id) - 1);
    target_nfc_id[sizeof(target_nfc_id) - 1] = '\0';

    // 새 화면 생성
    device_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(device_scr, lv_color_white(), 0);

    // 타이틀
    lv_obj_t *title = lv_label_create(device_scr);
    lv_label_set_text(title, "기기 선택");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &font_korean_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // NFC ID 표시
    lv_obj_t *nfc_label = lv_label_create(device_scr);
    char nfc_buf[64];
    snprintf(nfc_buf, sizeof(nfc_buf), "NFC: %s", nfc_id);
    lv_label_set_text(nfc_label, nfc_buf);
    lv_obj_set_style_text_color(nfc_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(nfc_label, &font_korean_16, 0);
    lv_obj_align(nfc_label, LV_ALIGN_TOP_MID, 0, 45);

    // 구분선
    lv_obj_t *divider = lv_obj_create(device_scr);
    lv_obj_set_size(divider, 200, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 70);

    // Supabase에서 모든 device_number 가져오기
    static char devices[20][32];
    int count = 0;
    bool ok = supabase_get_all_devices(devices, 20, &count);

    if (!ok || count == 0) {
        lv_obj_t *msg = lv_label_create(device_scr);
        lv_label_set_text(msg, "등록된 기기가\n없습니다");
        lv_obj_set_style_text_color(msg, lv_color_hex(0x999999), 0);
        lv_obj_set_style_text_font(msg, &font_korean_16, 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(msg);
    } else {
        lv_obj_t *list = lv_obj_create(device_scr);
        lv_obj_set_size(list, 220, 190);
        lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 80);
        lv_obj_set_style_bg_color(list, lv_color_white(), 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_pad_all(list, 5, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list, 8, 0);

        for (int i = 0; i < count; i++) {
            lv_obj_t *btn = lv_btn_create(list);
            lv_obj_set_size(btn, 200, 36);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x666666), LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 4, 0);

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, devices[i]);
            lv_obj_set_style_text_font(label, &font_korean_16, 0);
            lv_obj_center(label);

            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, devices[i]);
        }
    }

    // 결과 라벨
    result_label = lv_label_create(device_scr);
    lv_label_set_text(result_label, "");
    lv_obj_set_style_text_font(result_label, &font_korean_16, 0);
    lv_obj_align(result_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // 새 화면으로 전환
    lv_screen_load(device_scr);
}
