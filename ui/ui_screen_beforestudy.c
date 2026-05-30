#include "ui_screen_beforestudy.h"
#include <stdio.h>
#include <time.h>

LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

#define SCREEN_W 240
#define SCREEN_H 320

/* ── 상태 변수 ── */
static lv_obj_t *time_label;
static lv_timer_t *clock_timer;
static lv_obj_t *selected_item;   /* 현재 선택된 과목 카드 */
static lv_obj_t *remain_label;    /* 남은 시간 레이블 */

/* ── 과목 데이터 (예시) ── */
typedef struct {
    const char *name;
    const char *time_range;
    lv_color_t color;
} subject_t;

static const subject_t subjects[] = {
    {"경영데이터분석", "19:00 ~ 20:00", LV_COLOR_MAKE(0x4C, 0xAF, 0x50)},
    {"운영체제",       "20:00 ~ 21:00", LV_COLOR_MAKE(0xF4, 0x43, 0x36)},
    {"알고리즘",       "21:00 ~ 22:00", LV_COLOR_MAKE(0x21, 0x96, 0xF3)},
    {"네트워크",       "22:00 ~ 23:00", LV_COLOR_MAKE(0xFF, 0x98, 0x00)},
    {"자료구조",       "23:00 ~ 00:00", LV_COLOR_MAKE(0x9C, 0x27, 0xB0)},
};

#define SUBJECT_COUNT (sizeof(subjects) / sizeof(subjects[0]))

/* ── 시간 업데이트 콜백 ── */
static void update_time_cb(lv_timer_t *timer) {
    (void)timer;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(time_label, buf);

    /* 남은 시간 업데이트 (예시: 하드코딩, 추후 데이터 연동) */
    /* TODO: 실제 공부 시작/종료 시간과 비교하여 계산 */
    lv_label_set_text(remain_label, "4시간 00분 00초 전...");
}

/* ── 과목 선택 콜백 ── */
static void subject_click_cb(lv_event_t *e) {
    lv_obj_t *clicked = lv_event_get_target(e);

    /* 기존 선택 해제 */
    if (selected_item && selected_item != clicked) {
        lv_obj_set_style_bg_color(selected_item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(selected_item, 0, 0);
    }

    /* 새 선택 적용 */
    lv_obj_set_style_bg_color(clicked, lv_color_hex(0xF9F9DD), 0);
    lv_obj_set_style_border_width(clicked, 2, 0);
    lv_obj_set_style_border_color(clicked, lv_color_hex(0xBBBBBB), 0);
    selected_item = clicked;
}

/* ── 공부 시작 버튼 콜백 ── */
static void start_btn_cb(lv_event_t *e) {
    (void)e;
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    /* TODO: 선택된 과목으로 공부 시작 화면으로 전환 */
}

/* ── 과목 카드 생성 ── */
static lv_obj_t *create_subject_card(lv_obj_t *parent, const subject_t *subj) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, SCREEN_W - 20);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, subject_click_cb, LV_EVENT_CLICKED, NULL);

    /* 좌측 색상 아이콘 */
    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 12, 12);
    lv_obj_set_style_bg_color(icon, subj->color, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_radius(icon, 3, 0);

    /* 우측 텍스트 영역 */
    lv_obj_t *info = lv_obj_create(card);
    lv_obj_set_size(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(info, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_pad_all(info, 0, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *name_lbl = lv_label_create(info);
    lv_label_set_text(name_lbl, subj->name);
    lv_obj_set_style_text_font(name_lbl, &font_korean_16, 0);

    lv_obj_t *time_lbl = lv_label_create(info);
    lv_label_set_text(time_lbl, subj->time_range);
    lv_obj_set_style_text_font(time_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0x888888), 0);

    return card;
}

/* ── 화면 생성 ── */
void ui_screen_beforestudy_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ══════════════════════════════════════════
     * 1. 상단 영역: 현재 시각
     * ══════════════════════════════════════════ */
    lv_obj_t *top = lv_obj_create(scr);
    lv_obj_set_width(top, SCREEN_W);
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_top(top, 10, 0);
    lv_obj_set_style_pad_bottom(top, 2, 0);
    lv_obj_set_style_pad_left(top, 10, 0);
    lv_obj_set_style_pad_right(top, 10, 0);

    time_label = lv_label_create(top);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x222222), 0);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 0, 0);

    /* 구분선 */
    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(div1, 0, 0);

    /* ══════════════════════════════════════════
     * 2. 중단 영역: 과목 리스트 (스크롤)
     * ══════════════════════════════════════════ */
    lv_obj_t *mid = lv_obj_create(scr);
    lv_obj_set_width(mid, SCREEN_W);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mid, 0, 0);
    lv_obj_set_style_pad_top(mid, 5, 0);
    lv_obj_set_style_pad_bottom(mid, 0, 0);
    lv_obj_set_style_pad_left(mid, 5, 0);
    lv_obj_set_style_pad_right(mid, 5, 0);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(mid, LV_DIR_VER);
    lv_obj_set_style_pad_row(mid, 6, 0);

    for (int i = 0; i < (int)SUBJECT_COUNT; i++) {
        create_subject_card(mid, &subjects[i]);
    }

    /* ══════════════════════════════════════════
     * 3. 하단 영역: 버튼 + 남은 시간
     * ══════════════════════════════════════════ */
    /* 구분선 */
    lv_obj_t *div2 = lv_obj_create(scr);
    lv_obj_set_size(div2, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div2, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(div2, 0, 0);

    lv_obj_t *bottom = lv_obj_create(scr);
    lv_obj_set_width(bottom, SCREEN_W);
    lv_obj_set_height(bottom, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom, 0, 0);
    lv_obj_set_style_pad_top(bottom, 6, 0);
    lv_obj_set_style_pad_bottom(bottom, 8, 0);
    lv_obj_set_style_pad_left(bottom, 10, 0);
    lv_obj_set_style_pad_right(bottom, 10, 0);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(bottom, 6, 0);

    /* 공부 시작 버튼 */
    lv_obj_t *btn = lv_btn_create(bottom);
    lv_obj_set_size(btn, (int)(SCREEN_W * 0.80), 36);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xBBDEFB), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x90CAF9), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(btn, start_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "공부 시작");
    lv_obj_set_style_text_font(btn_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(btn_lbl);

    /* 남은 시간 레이블 */
    remain_label = lv_label_create(bottom);
    lv_label_set_text(remain_label, "4시간 00분 00초 전...");
    lv_obj_set_style_text_font(remain_label, &font_korean_16, 0);
    lv_obj_set_style_text_color(remain_label, lv_color_hex(0xF44336), 0);

    /* ── 타이머 시작 ── */
    clock_timer = lv_timer_create(update_time_cb, 1000, NULL);
    update_time_cb(NULL);

    lv_screen_load(scr);
}
