#include "ui_screen_beforestudy.h"
#include "ui_screen_studying.h"
#include "ui_screen_records.h"
#include "../app/app_supabase.h"
#include "../app/app_nfc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

#define SCREEN_W 240
#define SCREEN_H 320

/* ── UI 상태 ── */
static lv_obj_t   *time_label;
static lv_timer_t *clock_timer;
static lv_obj_t   *start_btn;
static lv_obj_t   *list_cont;
static int         selected_subject_idx;   /* -1 = 미선택 */
static lv_obj_t   *selected_card;         /* 현재 하이라이트된 카드 */

/* ── 비동기 데이터 ── */
static volatile bool  checklists_loading;
static volatile bool  checklists_ready;
static checklist_subject_group_t subject_groups[MAX_CHECKLIST_SUBJECTS];
static int              subject_group_count;
static lv_timer_t      *checklist_timer;
static bool             checklists_cached;

/* ── 과목별 UI 참조 (확장/축소, 라벨 갱신용) ── */
static lv_obj_t *count_labels[MAX_CHECKLIST_SUBJECTS];
static lv_obj_t *checklist_containers[MAX_CHECKLIST_SUBJECTS];

/* ── 화면 전환 인자 ── */
static studying_info_t pending_info;
static char            pending_progress_text[32];
static char            pending_subject_id[40];
static char            pending_nfc_id[40];

/* ── hex → lv_color_t ── */
static lv_color_t hex_to_color(const char *hex)
{
    if (!hex || hex[0] != '#') return lv_color_hex(0x4CAF50);
    unsigned long val = strtoul(hex + 1, NULL, 16);
    return lv_color_hex((uint32_t)val);
}

/* ── 시계 업데이트 ── */
static void update_time_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(time_label, buf);
}

/* ── 기록확인 버튼 콜백 → ui_screen_records ── */
static void records_btn_cb(lv_event_t *e)
{
    (void)e;

    if (clock_timer)     { lv_timer_delete(clock_timer);     clock_timer     = NULL; }
    if (checklist_timer) { lv_timer_delete(checklist_timer); checklist_timer = NULL; }

    time_label    = NULL;
    start_btn     = NULL;
    list_cont     = NULL;
    selected_card = NULL;

    ui_screen_records_create(LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

/* ── 상단 행 (시계 + 기록확인 버튼) ── */
static lv_obj_t *create_top_row(lv_obj_t *scr)
{
    lv_obj_t *top = lv_obj_create(scr);
    lv_obj_set_width(top, SCREEN_W);
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_top(top, 10, 0);
    lv_obj_set_style_pad_bottom(top, 2, 0);
    lv_obj_set_style_pad_left(top, 10, 0);
    lv_obj_set_style_pad_right(top, 10, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    time_label = lv_label_create(top);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x222222), 0);

    lv_obj_t *records_btn = lv_btn_create(top);
    lv_obj_set_size(records_btn, 100, 40);
    lv_obj_set_style_bg_color(records_btn, lv_color_hex(0xFFE0B2), 0);
    lv_obj_set_style_bg_color(records_btn, lv_color_hex(0xFFCC80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(records_btn, 8, 0);
    lv_obj_set_style_border_width(records_btn, 1, 0);
    lv_obj_set_style_border_color(records_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(records_btn, records_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *records_lbl = lv_label_create(records_btn);
    lv_label_set_text(records_lbl, "기록확인->");
    lv_obj_set_style_text_font(records_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(records_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(records_lbl);

    return top;
}

/* ── 체크리스트 항목 클릭 (읽기 전용: 체크 불가) ── */

/* ── 과목 카드 클릭 → 체크리스트 expand/collapse ── */
static void subject_card_click_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_current_target(e);
    int gi = (int)(intptr_t)lv_obj_get_user_data(card);

    if (gi < 0 || gi >= subject_group_count) return;
    lv_obj_t *cl = checklist_containers[gi];
    if (!cl) return;

    if (lv_obj_has_flag(cl, LV_OBJ_FLAG_HIDDEN)) {
        /* 다른 열린 체크리스트 접기 */
        for (int i = 0; i < subject_group_count; i++) {
            if (i != gi && checklist_containers[i]) {
                lv_obj_add_flag(checklist_containers[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        /* 이전 선택 카드 하이라이트 해제 */
        if (selected_card && selected_card != card) {
            lv_obj_set_style_bg_color(selected_card, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(selected_card, 0, 0);
        }

        /* 현재 카드 하이라이트 */
        lv_obj_set_style_bg_color(card, lv_color_hex(0xF9F9DD), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xBBBBBB), 0);
        selected_card = card;
        selected_subject_idx = gi;

        /* 체크리스트 표시 */
        lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);

        /* 시작 버튼 활성화 */
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xBBDEFB), 0);
        lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        /* 접기 */
        lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(card, 0, 0);
        selected_card = NULL;
        selected_subject_idx = -1;

        /* 시작 버튼 비활성화 */
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xCCCCCC), 0);
        lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

/* ── 과목 카드 + 체크리스트 생성 ── */
static void create_subject_card_with_checklist(lv_obj_t *parent, int gi)
{
    checklist_subject_group_t *g = &subject_groups[gi];
    lv_color_t color = hex_to_color(g->color_hex);

    /* 카드 컨테이너 */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, SCREEN_W - 20);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, subject_card_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(card, (void *)(intptr_t)gi);

    /* 헤더: 아이콘 + 과목명 (한 줄) */
    lv_obj_t *hdr = lv_obj_create(card);
    lv_obj_set_size(hdr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, 8, 0);

    /* 색상 아이콘 */
    lv_obj_t *icon = lv_obj_create(hdr);
    lv_obj_set_size(icon, 14, 14);
    lv_obj_set_style_bg_color(icon, color, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_radius(icon, 4, 0);

    /* 과목명 */
    lv_obj_t *name_lbl = lv_label_create(hdr);
    lv_label_set_text(name_lbl, g->subject_name);
    lv_obj_set_style_text_font(name_lbl, &font_korean_20, 0);

    /* "N/M 완료" (과목명 아래) */
    char count_buf[24];
    snprintf(count_buf, sizeof(count_buf), "%d/%d 완료",
             g->checked_count, g->item_count);
    lv_obj_t *count_lbl = lv_label_create(card);
    lv_label_set_text(count_lbl, count_buf);
    lv_obj_set_style_text_font(count_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(count_lbl, lv_color_hex(0x888888), 0);
    count_labels[gi] = count_lbl;

    /* 체크리스트 컨테이너 (처음엔 HIDDEN) */
    lv_obj_t *cl = lv_obj_create(card);
    lv_obj_set_width(cl, SCREEN_W - 40);
    lv_obj_set_height(cl, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cl, 0, 0);
    lv_obj_set_style_pad_all(cl, 0, 0);
    lv_obj_set_flex_flow(cl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cl, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cl, 2, 0);
    lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN);
    checklist_containers[gi] = cl;

    /* 체크리스트 항목 */
    for (int ii = 0; ii < g->item_count; ii++) {
        lv_obj_t *cb = lv_checkbox_create(cl);
        lv_checkbox_set_text(cb, g->items[ii].text);
        lv_obj_set_style_text_font(cb, &font_korean_16, 0);
        lv_obj_set_width(cb, SCREEN_W - 44);

        if (g->items[ii].is_checked) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
        }
        /* 읽기 전용: 체크 불가, 터치 무시 */
        lv_obj_clear_flag(cb, LV_OBJ_FLAG_CLICKABLE);
    }
}

/* ── UI 업데이트 (데이터 → 화면) ── */
static void update_checklist_ui(void)
{
    lv_obj_clean(list_cont);

    for (int i = 0; i < MAX_CHECKLIST_SUBJECTS; i++) {
        count_labels[i] = NULL;
        checklist_containers[i] = NULL;
    }

    if (subject_group_count == 0) {
        lv_obj_t *empty = lv_label_create(list_cont);
        lv_label_set_text(empty, "오늘 체크리스트가 없습니다");
        lv_obj_set_style_text_font(empty, &font_korean_16, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        return;
    }

    for (int i = 0; i < subject_group_count; i++) {
        create_subject_card_with_checklist(list_cont, i);
    }
}

/* ── 공부 시작 버튼 ── */
static void start_btn_cb(lv_event_t *e)
{
    (void)e;
    if (selected_subject_idx < 0 || selected_subject_idx >= subject_group_count)
        return;

    /* 타이머 정리 */
    if (clock_timer)     { lv_timer_delete(clock_timer);     clock_timer     = NULL; }
    if (checklist_timer) { lv_timer_delete(checklist_timer); checklist_timer = NULL; }

    checklist_subject_group_t *g = &subject_groups[selected_subject_idx];

    /* subject_id 복사 */
    strncpy(pending_subject_id, g->subject_id, sizeof(pending_subject_id) - 1);
    pending_subject_id[sizeof(pending_subject_id) - 1] = '\0';

    pending_info.subject_name         = g->subject_name;
    pending_info.subject_id           = pending_subject_id;
    pending_info.color_hex            = g->color_hex;
    pending_info.color                = hex_to_color(g->color_hex);
    /* NFC ID 캡처: 폰이 NFC 리더 위에 있는 상태에서 studying 으로 진입.
     * g_nfc.uid_str 이 비어있으면 resume 매칭이 불가능하므로 빈 문자열로 두어
     * pause check 시 항상 매칭 실패하도록 한다. */
    if (g_nfc.tag_detected && g_nfc.uid_str[0]) {
        strncpy(pending_nfc_id, (const char *)g_nfc.uid_str, sizeof(pending_nfc_id) - 1);
        pending_nfc_id[sizeof(pending_nfc_id) - 1] = '\0';
    } else {
        pending_nfc_id[0] = '\0';
    }
    pending_info.nfc_id               = pending_nfc_id;
    pending_info.checklist_items      = g->items;
    pending_info.checklist_item_count = g->item_count;
    pending_info.checklist_checked_count = g->checked_count;

    /* 댕글링 방지 */
    time_label = NULL;
    start_btn  = NULL;
    list_cont  = NULL;

    ui_screen_studying_create(&pending_info);
}

/* ── Supabase 폴링 타이머 ── */
static void checklist_load_timer_cb(lv_timer_t *timer)
{
    if (checklists_loading) return;

    lv_timer_delete(timer);
    checklist_timer = NULL;

    if (checklists_ready) {
        checklists_cached = true;
        update_checklist_ui();
    } else {
        lv_obj_clean(list_cont);
        lv_obj_t *err = lv_label_create(list_cont);
        lv_label_set_text(err, "불러오기 실패");
        lv_obj_set_style_text_font(err, &font_korean_16, 0);
        lv_obj_set_style_text_color(err, lv_color_hex(0xF44336), 0);
    }
}

/* ── 백그라운드 스레드 ── */
static void *fetch_checklists_thread(void *arg)
{
    (void)arg;
    checklists_ready = supabase_get_today_checklists(
        subject_groups, MAX_CHECKLIST_SUBJECTS, &subject_group_count);
    checklists_loading = false;
    if (!checklists_ready) checklists_cached = false;
    return NULL;
}

/* ── 화면 생성 ── */
void ui_screen_beforestudy_invalidate_cache(void)
{
    checklists_cached = false;
    subject_group_count = 0;
}

void ui_screen_beforestudy_create(lv_scr_load_anim_t anim)
{
    /* ── 캐시 유효 시: 네트워크 없이 즉시 복원 ── */
    if (checklists_cached && subject_group_count > 0) {
        if (checklist_timer) { lv_timer_delete(checklist_timer); checklist_timer = NULL; }
        if (clock_timer)     { lv_timer_delete(clock_timer);     clock_timer     = NULL; }

        selected_card = NULL;
        selected_subject_idx = -1;

        lv_obj_t *scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        /* 상단: 시계 + 기록확인 버튼 */
        lv_obj_t *top = create_top_row(scr);

        lv_obj_t *div1 = lv_obj_create(scr);
        lv_obj_set_size(div1, SCREEN_W, 1);
        lv_obj_set_style_bg_color(div1, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_width(div1, 0, 0);

        /* 중단: 과목 리스트 */
        list_cont = lv_obj_create(scr);
        lv_obj_set_width(list_cont, SCREEN_W);
        lv_obj_set_flex_grow(list_cont, 1);
        lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list_cont, 0, 0);
        lv_obj_set_style_pad_top(list_cont, 5, 0);
        lv_obj_set_style_pad_bottom(list_cont, 0, 0);
        lv_obj_set_style_pad_left(list_cont, 5, 0);
        lv_obj_set_style_pad_right(list_cont, 5, 0);
        lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
        lv_obj_set_style_pad_row(list_cont, 6, 0);

        update_checklist_ui();

        /* 하단: 시작 버튼 */
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
        lv_obj_set_style_pad_bottom(bottom, 10, 0);
        lv_obj_set_style_pad_left(bottom, 10, 0);
        lv_obj_set_style_pad_right(bottom, 10, 0);
        lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        start_btn = lv_btn_create(bottom);
        lv_obj_set_size(start_btn, (int)(SCREEN_W * 0.45), 48);
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x90CAF9), LV_STATE_PRESSED);
        lv_obj_set_style_radius(start_btn, 10, 0);
        lv_obj_set_style_border_width(start_btn, 1, 0);
        lv_obj_set_style_border_color(start_btn, lv_color_hex(0x222222), 0);
        lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *btn_lbl = lv_label_create(start_btn);
        lv_label_set_text(btn_lbl, "공부 시작");
        lv_obj_set_style_text_font(btn_lbl, &font_korean_16, 0);
        lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x222222), 0);
        lv_obj_center(btn_lbl);

        clock_timer = lv_timer_create(update_time_cb, 1000, NULL);
        update_time_cb(NULL);

        lv_screen_load_anim(scr, anim, 300, 0, true);
        return;
    }

    /* ── 캐시 없음: 네트워크에서 새로 로드 ── */
    if (checklist_timer) { lv_timer_delete(checklist_timer); checklist_timer = NULL; }
    if (clock_timer)     { lv_timer_delete(clock_timer);     clock_timer     = NULL; }

    selected_card = NULL;
    selected_subject_idx = -1;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 상단: 시계 + 기록확인 버튼 */
    lv_obj_t *top = create_top_row(scr);

    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(div1, 0, 0);

    /* 중단: 로딩 표시 */
    list_cont = lv_obj_create(scr);
    lv_obj_set_width(list_cont, SCREEN_W);
    lv_obj_set_flex_grow(list_cont, 1);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_cont, 0, 0);
    lv_obj_set_style_pad_top(list_cont, 5, 0);
    lv_obj_set_style_pad_bottom(list_cont, 0, 0);
    lv_obj_set_style_pad_left(list_cont, 5, 0);
    lv_obj_set_style_pad_right(list_cont, 5, 0);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
    lv_obj_set_style_pad_row(list_cont, 6, 0);

    lv_obj_t *loading = lv_label_create(list_cont);
    lv_label_set_text(loading, "불러오는 중...");
    lv_obj_set_style_text_font(loading, &font_korean_16, 0);
    lv_obj_set_style_text_color(loading, lv_color_hex(0x888888), 0);

    /* 하단: 시작 버튼 */
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
    lv_obj_set_style_pad_bottom(bottom, 10, 0);
    lv_obj_set_style_pad_left(bottom, 10, 0);
    lv_obj_set_style_pad_right(bottom, 10, 0);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    start_btn = lv_btn_create(bottom);
    lv_obj_set_size(start_btn, (int)(SCREEN_W * 0.45), 48);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x90CAF9), LV_STATE_PRESSED);
    lv_obj_set_style_radius(start_btn, 10, 0);
    lv_obj_set_style_border_width(start_btn, 1, 0);
    lv_obj_set_style_border_color(start_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_lbl = lv_label_create(start_btn);
    lv_label_set_text(btn_lbl, "공부 시작");
    lv_obj_set_style_text_font(btn_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(btn_lbl);

    /* 시계 타이머 */
    clock_timer = lv_timer_create(update_time_cb, 1000, NULL);
    update_time_cb(NULL);

    /* 비동기 로드 */
    checklists_loading = true;
    checklists_ready   = false;
    subject_group_count = 0;

    pthread_t tid;
    pthread_create(&tid, NULL, fetch_checklists_thread, NULL);
    pthread_detach(tid);

    checklist_timer = lv_timer_create(checklist_load_timer_cb, 100, NULL);

    lv_screen_load_anim(scr, anim, 300, 0, true);
}
