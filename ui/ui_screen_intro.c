#include "ui_screen_intro.h"
#include <stdio.h>
#include "app/app_hx711.h"
#include "app/app_nfc.h"

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);
LV_FONT_DECLARE(lv_font_montserrat_32);

enum { STATE_IDLE, STATE_RECOGNIZING, STATE_WEIGHT_DONE, STATE_NFC_WAIT, STATE_NFC_DONE };

static lv_obj_t *status_label;
static lv_obj_t *guide_label;
static lv_obj_t *spinner;
static lv_obj_t *nfc_uid_label;
static int state = STATE_IDLE;
static uint32_t recognize_start = 0;

static void remove_spinner(void) {
    if (spinner) {
        lv_obj_delete(spinner);
        spinner = NULL;
    }
}

static void go_idle(void) {
    state = STATE_IDLE;
    remove_spinner();
    if (nfc_uid_label) {
        lv_obj_delete(nfc_uid_label);
        nfc_uid_label = NULL;
    }
    lv_label_set_text(status_label, "대기 중...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
    lv_obj_clear_flag(guide_label, LV_OBJ_FLAG_HIDDEN);
}

static void transition_to_nfc_cb(lv_timer_t *timer) {
    (void)timer;
    state = STATE_NFC_WAIT;
    lv_label_set_text(status_label, "NFC 인식 중...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
    spinner = lv_spinner_create(lv_screen_active());
    lv_spinner_set_anim_params(spinner, 1000, 200);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 80);
}

static void weight_timer_cb(lv_timer_t *timer) {
    if (state == STATE_NFC_WAIT || state == STATE_NFC_DONE) return;
    if (!hx711_is_ready()) return;

    float weight = hx711_read_weight();
    if (weight < 0) weight = 0;

    switch (state) {
    case STATE_IDLE:
        if (weight >= 100) {
            state = STATE_RECOGNIZING;
            recognize_start = lv_tick_get();
            lv_label_set_text(status_label, "인식 중...");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
            lv_obj_add_flag(guide_label, LV_OBJ_FLAG_HIDDEN);
            spinner = lv_spinner_create(lv_screen_active());
            lv_spinner_set_anim_params(spinner, 1000, 200);
            lv_obj_set_size(spinner, 40, 40);
            lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 80);
        }
        break;

    case STATE_RECOGNIZING:
        if (weight < 100) {
            go_idle();
        } else if (lv_tick_elaps(recognize_start) >= 3000) {
            state = STATE_WEIGHT_DONE;
            remove_spinner();
            lv_label_set_text(status_label, "인식 완료!");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x2E7D32), 0);
            // 1.5초 후 NFC 단계로
            lv_timer_t *t = lv_timer_create(transition_to_nfc_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
        }
        break;

    default:
        break;
    }
}

static void nfc_timer_cb(lv_timer_t *timer) {
    if (state != STATE_NFC_WAIT) return;

    if (g_nfc.tag_detected) {
        state = STATE_NFC_DONE;
        remove_spinner();
        lv_label_set_text(status_label, "NFC 인식 완료!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x2E7D32), 0);

        nfc_uid_label = lv_label_create(lv_screen_active());
        lv_label_set_text(nfc_uid_label, (const char *)g_nfc.uid_str);
        lv_obj_set_style_text_color(nfc_uid_label, lv_color_hex(0x333333), 0);
        lv_obj_set_style_text_font(nfc_uid_label, &font_korean_16, 0);
        lv_obj_align(nfc_uid_label, LV_ALIGN_CENTER, 0, 80);
    }
}

void create_study_manager_ui(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── 1. 상단 타이틀 ──────────────────────────
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "STUDY\nMANAGER");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

    // ── 2. 구분선 ───────────────────────────────
    lv_obj_t *divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 160, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 115);

    // ── 3. 중앙 상태 텍스트 ──────────────────────
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "대기 중...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(status_label, &font_korean_20, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 20);

    // 500ms마다 무게 체크
    lv_timer_create(weight_timer_cb, 500, NULL);
    // 300ms마다 NFC 체크
    lv_timer_create(nfc_timer_cb, 300, NULL);

    // ── 4. 하단 안내 문구 ────────────────────────
    guide_label = lv_label_create(scr);
    lv_label_set_text(guide_label, "스마트폰을\n상자 위에 올려주세요");
    lv_obj_set_style_text_color(guide_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(guide_label, &font_korean_16, 0);
    lv_obj_set_style_text_align(guide_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(guide_label, LV_ALIGN_BOTTOM_MID, 0, -45);
}
