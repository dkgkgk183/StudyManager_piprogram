#define _GNU_SOURCE
#include "ui_screen_records.h"
#include "ui_screen_beforestudy.h"
#include "ui_screen_done.h"
#include "../app/app_sessions.h"
#include "../app/app_supabase.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

#define SCREEN_W 240
#define SCREEN_H 320

static lv_obj_t   *time_label;
static lv_timer_t *clock_timer;

/* ── 롱프레스 액션 버튼 상태 ── */
static lv_obj_t *active_card   = NULL;   /* 현재 액션 버튼이 열린 카드 */
static lv_obj_t *active_btn_row = NULL;  /* 액션 버튼 컨테이너 */

/* ── 공부완료 팝업 / 업로드 상태 ── */
static lv_obj_t     *popup_overlay = NULL;
static lv_obj_t     *popup_card    = NULL;
static volatile bool upload_running = false;
static lv_timer_t   *upload_poll_timer = NULL;
static int           upload_success_count;
static int           upload_failed_count;
static int           upload_total;

static void update_time_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(time_label, buf);
}

/* 현재 시각 기준 공부일 [start, end) 윈도우: KST 06:00 ~ 익일 06:00
 * (beforestudy의 get_study_day_utc_range와 동일 로직, Unix timestamp 반환) */
static void get_study_day_window(time_t *win_start, time_t *win_end)
{
    time_t now     = time(NULL);
    time_t kst_now = now + 9 * 3600;
    struct tm kst;
    gmtime_r(&kst_now, &kst);

    int year = kst.tm_year + 1900;
    int mon  = kst.tm_mon + 1;
    int mday = kst.tm_mday;

    if (kst.tm_hour < 6) {
        mday -= 1;
        if (mday < 1) {
            mon -= 1;
            if (mon < 1) { mon = 12; year -= 1; }
            static const int dim[] = {31,28,31,30,31,30,31,30,31,30,31,30,31};
            mday = dim[mon - 1];
            if (mon == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
                mday = 29;
        }
    }

    struct tm kst_start = {0};
    kst_start.tm_year = year - 1900;
    kst_start.tm_mon  = mon - 1;
    kst_start.tm_mday = mday;
    kst_start.tm_hour = 6;
    kst_start.tm_min  = 0;
    kst_start.tm_sec  = 0;

    *win_start = timegm(&kst_start) - 9 * 3600;
    *win_end   = *win_start + 24 * 3600;
}

/* ── 공부완료 팝업 ── */

static void close_popup(void)
{
    if (popup_card)    { lv_obj_del(popup_card);    popup_card    = NULL; }
    if (popup_overlay) { lv_obj_del(popup_overlay); popup_overlay = NULL; }
}

static void popup_create_base(const char *message)
{
    lv_obj_t *scr = lv_screen_active();

    popup_overlay = lv_obj_create(scr);
    lv_obj_add_flag(popup_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);  /* 화면 flex 레이아웃에서 제외 */
    lv_obj_set_size(popup_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(popup_overlay, 0, 0);
    lv_obj_set_style_bg_color(popup_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(popup_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(popup_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(popup_overlay);  /* z-order 맨 위 */

    popup_card = lv_obj_create(popup_overlay);
    lv_obj_set_size(popup_card, 220, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(popup_card, 170, 0);
    lv_obj_center(popup_card);
    lv_obj_set_style_bg_color(popup_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(popup_card, 12, 0);
    lv_obj_set_style_border_color(popup_card, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(popup_card, 2, 0);
    lv_obj_set_flex_flow(popup_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(popup_card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(popup_card, 16, 0);
    lv_obj_set_style_pad_row(popup_card, 18, 0);

    lv_obj_t *msg = lv_label_create(popup_card);
    lv_obj_set_style_text_font(msg, &font_korean_16, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, lv_pct(100));
    lv_label_set_text(msg, message);
}

static void show_confirm_popup(const char *message,
                                lv_event_cb_t on_yes, lv_event_cb_t on_no)
{
    popup_create_base(message);

    lv_obj_t *btn_row = lv_obj_create(popup_card);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 16, 0);

    lv_obj_t *no_btn = lv_btn_create(btn_row);
    lv_obj_set_size(no_btn, 70, 38);
    lv_obj_set_style_bg_color(no_btn, lv_color_hex(0xBDBDBD), 0);
    lv_obj_set_style_bg_color(no_btn, lv_color_hex(0x9E9E9E), LV_STATE_PRESSED);
    lv_obj_set_style_radius(no_btn, 6, 0);
    lv_obj_add_event_cb(no_btn, on_no, LV_EVENT_CLICKED, NULL);
    lv_obj_t *no_lbl = lv_label_create(no_btn);
    lv_label_set_text(no_lbl, "아니오");
    lv_obj_set_style_text_font(no_lbl, &font_korean_16, 0);
    lv_obj_center(no_lbl);

    lv_obj_t *yes_btn = lv_btn_create(btn_row);
    lv_obj_set_size(yes_btn, 70, 38);
    lv_obj_set_style_bg_color(yes_btn, lv_color_hex(0x90CAF9), 0);
    lv_obj_set_style_bg_color(yes_btn, lv_color_hex(0x64B5F6), LV_STATE_PRESSED);
    lv_obj_set_style_radius(yes_btn, 6, 0);
    lv_obj_add_event_cb(yes_btn, on_yes, LV_EVENT_CLICKED, NULL);
    lv_obj_t *yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, "예");
    lv_obj_set_style_text_font(yes_lbl, &font_korean_16, 0);
    lv_obj_center(yes_lbl);
}

static void show_result_popup(const char *message, lv_event_cb_t on_close)
{
    popup_create_base(message);

    if (!on_close) return;

    lv_obj_t *ok_btn = lv_btn_create(popup_card);
    lv_obj_set_size(ok_btn, 90, 38);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x90CAF9), 0);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x64B5F6), LV_STATE_PRESSED);
    lv_obj_set_style_radius(ok_btn, 6, 0);
    lv_obj_add_event_cb(ok_btn, on_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "확인");
    lv_obj_set_style_text_font(ok_lbl, &font_korean_16, 0);
    lv_obj_center(ok_lbl);
}

/* ── 업로드 스레드 / 폴 타이머 ── */

static void *upload_thread(void *arg)
{
    (void)arg;
    upload_success_count = 0;
    upload_failed_count  = 0;

    local_session_t all[MAX_LOCAL_SESSIONS];
    int count = sessions_load_all(all, MAX_LOCAL_SESSIONS);
    upload_total = count;

    for (int i = 0; i < count; i++) {
        if (all[i].pushed) continue;
        if (supabase_upload_session(&all[i])) {
            sessions_mark_pushed(i);
            upload_success_count++;
        } else {
            upload_failed_count++;
        }
    }

    upload_running = false;
    return NULL;
}

static void upload_poll_cb(lv_timer_t *t)
{
    if (upload_running) return;

    lv_timer_delete(t);
    upload_poll_timer = NULL;

    close_popup();

    if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }
    time_label = NULL;

    ui_screen_done_create(10);
}

static void upload_yes_cb(lv_event_t *e)
{
    (void)e;
    close_popup();

    int unpushed = sessions_get_unpushed_count();
    if (unpushed == 0) {
        if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }
        time_label = NULL;
        ui_screen_done_create(10);
        return;
    }

    show_result_popup("전송 중...", NULL);

    upload_running = true;
    pthread_t tid;
    pthread_create(&tid, NULL, upload_thread, NULL);
    pthread_detach(tid);
    upload_poll_timer = lv_timer_create(upload_poll_cb, 100, NULL);
}

static void upload_no_cb(lv_event_t *e)
{
    (void)e;
    close_popup();
}

static void done_btn_cb(lv_event_t *e)
{
    (void)e;
    if (upload_running) return;

    int unpushed = sessions_get_unpushed_count();
    if (unpushed == 0) {
        if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }
        time_label = NULL;
        ui_screen_done_create(10);
        return;
    }

    show_confirm_popup("모든 공부기록이 전송됩니다, 계속하시겠습니까?",
                       upload_yes_cb, upload_no_cb);
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;

    if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }
    if (upload_poll_timer) { lv_timer_delete(upload_poll_timer); upload_poll_timer = NULL; }

    time_label = NULL;

    ui_screen_beforestudy_create(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

/* ── 시간 포맷: 초 → 보기 좋은 형태 ── */
static void format_duration(char *buf, size_t len, int sec)
{
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    if (h > 0)
        snprintf(buf, len, "%d시간 %02d분", h, m);
    else
        snprintf(buf, len, "%d분 %02d초", m, s);
}

/* ── 액션 버튼 닫기 ── */
static void dismiss_action_buttons(void)
{
    if (active_btn_row) {
        lv_obj_del(active_btn_row);
        active_btn_row = NULL;
    }
    if (active_card) {
        lv_obj_set_style_border_color(active_card, lv_color_hex(0xDDDDDD), 0);
        active_card = NULL;
    }
}

/* ── 액션 버튼 닫기 콜백 (취소) ── */
static void cancel_action_cb(lv_event_t *e)
{
    (void)e;
    dismiss_action_buttons();
}

/* ── 삭제 콜백 ── */
static void delete_action_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    sessions_delete(idx);
    dismiss_action_buttons();

    /* 화면 갱신 */
    ui_screen_records_create(LV_SCR_LOAD_ANIM_NONE);
}

/* ── 롱프레스 콜백: 액션 버튼 표시 ── */
static void card_long_press_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(card);

    /* 이미 열린 카드면 무시 */
    if (active_card == card) return;

    /* 다른 카드에 열려있으면 닫기 */
    dismiss_action_buttons();

    /* 카드 테두리 강조 */
    lv_obj_set_style_border_color(card, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_border_width(card, 2, 0);

    /* 버튼 행 생성 */
    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_set_width(btn_row, SCREEN_W - 40);
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_style_pad_top(btn_row, 4, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 8, 0);

    /* 취소 버튼 */
    lv_obj_t *cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(cancel_btn, 60, 30);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xBDBDBD), 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x9E9E9E), LV_STATE_PRESSED);
    lv_obj_set_style_radius(cancel_btn, 6, 0);
    lv_obj_add_event_cb(cancel_btn, cancel_action_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "취소");
    lv_obj_set_style_text_font(cancel_lbl, &font_korean_16, 0);
    lv_obj_center(cancel_lbl);

    /* 삭제 버튼 */
    lv_obj_t *del_btn = lv_btn_create(btn_row);
    lv_obj_set_size(del_btn, 60, 30);
    lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xEF5350), 0);
    lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xC62828), LV_STATE_PRESSED);
    lv_obj_set_style_radius(del_btn, 6, 0);
    lv_obj_set_user_data(del_btn, (void *)(intptr_t)idx);
    lv_obj_add_event_cb(del_btn, delete_action_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *del_lbl = lv_label_create(del_btn);
    lv_label_set_text(del_lbl, "삭제");
    lv_obj_set_style_text_font(del_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(del_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(del_lbl);

    active_card = card;
    active_btn_row = btn_row;
}

/* ── 카드 생성 헬퍼 ── */
static lv_obj_t *create_session_card(lv_obj_t *parent, local_session_t *s, int index)
{
    /* ── 카드 ── */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, SCREEN_W - 20);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 2, 0);

    /* 롱프레스 인터랙션 */
    lv_obj_set_user_data(card, (void *)(intptr_t)index);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, card_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    /* 1행: 과목명 */
    lv_obj_t *name_row = lv_obj_create(card);
    lv_obj_set_size(name_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(name_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_row, 0, 0);
    lv_obj_set_style_pad_all(name_row, 0, 0);
    lv_obj_set_flex_flow(name_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(name_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(name_row, 6, 0);

    lv_obj_t *dot = lv_obj_create(name_row);
    lv_obj_set_size(dot, 10, 10);
    /* 색상 변환: "#FF5722" → 0xFF5722 */
    unsigned long color_val = strtoul(s->color_hex + 1, NULL, 16);
    lv_obj_set_style_bg_color(dot, lv_color_hex((uint32_t)color_val), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 3, 0);

    lv_obj_t *name_lbl = lv_label_create(name_row);
    lv_label_set_text(name_lbl, s->subject_name);
    lv_obj_set_style_text_font(name_lbl, &font_korean_20, 0);

    /* 2행: 날짜 + 시간 */
    struct tm tm_start = {0}, tm_end = {0};
    strptime(s->start_time, "%Y-%m-%dT%H:%M:%S", &tm_start);
    strptime(s->end_time, "%Y-%m-%dT%H:%M:%S", &tm_end);

    char time_buf[48];
    snprintf(time_buf, sizeof(time_buf), "%02d/%02d %02d:%02d ~ %02d:%02d",
             tm_start.tm_mon + 1, tm_start.tm_mday,
             tm_start.tm_hour, tm_start.tm_min,
             tm_end.tm_hour, tm_end.tm_min);

    lv_obj_t *time_lbl = lv_label_create(card);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_set_style_text_font(time_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0x666666), 0);

    /* 3행: 공부 시간 */
    char dur_buf[32];
    format_duration(dur_buf, sizeof(dur_buf), s->duration_seconds);

    char dur_label_buf[48];
    snprintf(dur_label_buf, sizeof(dur_label_buf), "공부시간: %s", dur_buf);

    lv_obj_t *dur_lbl = lv_label_create(card);
    lv_label_set_text(dur_lbl, dur_label_buf);
    lv_obj_set_style_text_font(dur_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(dur_lbl, lv_color_hex(0x888888), 0);

    return card;
}

void ui_screen_records_create(lv_scr_load_anim_t anim)
{
    /* 이전 액션 버튼 정리 */
    active_card = NULL;
    active_btn_row = NULL;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ── 상단: 시계 + 돌아가기 버튼 ── */
    lv_obj_t *top = lv_obj_create(scr);
    lv_obj_set_width(top, SCREEN_W);
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_top(top, 8, 0);
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

    lv_obj_t *back_btn = lv_btn_create(top);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xC5E1A5), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xA5D6A7), LV_STATE_PRESSED);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "<-공부계획");
    lv_obj_set_style_text_font(back_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(back_lbl);

    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(div1, 0, 0);

    /* ── 기록 리스트 (스크롤 영역) ── */
    lv_obj_t *list_cont = lv_obj_create(scr);
    lv_obj_set_width(list_cont, SCREEN_W);
    lv_obj_set_flex_grow(list_cont, 1);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_cont, 0, 0);
    lv_obj_set_style_pad_top(list_cont, 4, 0);
    lv_obj_set_style_pad_bottom(list_cont, 4, 0);
    lv_obj_set_style_pad_left(list_cont, 10, 0);
    lv_obj_set_style_pad_right(list_cont, 10, 0);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
    lv_obj_set_style_pad_row(list_cont, 6, 0);

    /* ── 공부일 윈도우 계산 + 만료 세션 정리 (KST 06:00 ~ 익일 06:00) ── */
    time_t win_start, win_end;
    get_study_day_window(&win_start, &win_end);
    sessions_prune_before(win_start);

    /* ── 세션 로드 ── */
    local_session_t sessions[MAX_LOCAL_SESSIONS];
    int count = sessions_load_all(sessions, MAX_LOCAL_SESSIONS);

    int displayed = 0;
    /* 최신순으로 표시 (윈도우 내 + 미전송 세션만) */
    for (int i = count - 1; i >= 0; i--) {
        if (sessions[i].pushed) continue;   /* 전송 완료된 세션은 숨김 */
        struct tm tm_end = {0};
        if (strptime(sessions[i].end_time, "%Y-%m-%dT%H:%M:%S", &tm_end) == NULL) continue;
        time_t end_t = mktime(&tm_end);
        if (end_t < win_start || end_t >= win_end) continue;
        create_session_card(list_cont, &sessions[i], i);
        displayed++;
    }

    if (displayed == 0) {
        lv_obj_t *empty = lv_label_create(list_cont);
        lv_label_set_text(empty, "공부 기록이 없습니다");
        lv_obj_set_style_text_font(empty, &font_korean_16, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x999999), 0);
    }

    /* ── 하단: 구분선 + 완료 버튼 ── */
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
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *done_btn = lv_btn_create(bottom);
    lv_obj_set_size(done_btn, SCREEN_W - 20, 48);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0x90CAF9), LV_STATE_PRESSED);
    lv_obj_set_style_radius(done_btn, 10, 0);
    lv_obj_set_style_border_width(done_btn, 1, 0);
    lv_obj_set_style_border_color(done_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(done_btn, done_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *done_lbl = lv_label_create(done_btn);
    lv_label_set_text(done_lbl, "공부 완료");
    lv_obj_set_style_text_font(done_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(done_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(done_lbl);

    clock_timer = lv_timer_create(update_time_cb, 1000, NULL);
    update_time_cb(NULL);

    lv_screen_load_anim(scr, anim, 300, 0, true);
}
