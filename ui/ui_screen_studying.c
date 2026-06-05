#include "ui_screen_studying.h"
#include "ui_screen_beforestudy.h"
#include "../app/app_sessions.h"
#include "../app/app_supabase.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);
LV_FONT_DECLARE(font_korean_20_bold);

#define SCREEN_W 240
#define SCREEN_H 320

/* ── 상태 변수 ── */
static lv_obj_t   *scr_ref;
static lv_obj_t   *time_label;
static lv_obj_t   *timer_label;
static lv_obj_t   *progress_label;
static lv_timer_t *tick_timer;
static lv_timer_t *clock_timer;

static int  elapsed_sec;
static int  checked_count;
static int  total_count;

/* ── 세션 저장용 ── */
static time_t study_start_time;
static char   saved_subject_name[64];
static char   saved_subject_id[40];
static char   saved_color_hex[8];
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

/* ── 시간 포맷 ── */
static void format_hms(char *buf, size_t len, int sec)
{
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    snprintf(buf, len, "%02d:%02d:%02d", h, m, s);
}

/* ── 1초 스톱워치 콜백 ── */
static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    elapsed_sec++;

    char buf[16];
    format_hms(buf, sizeof(buf), elapsed_sec);
    lv_label_set_text(timer_label, buf);
}

/* ── 체크박스 클릭 → Supabase PATCH + 진행 갱신 ── */
static void checklist_item_click_cb(lv_event_t *e)
{
    lv_obj_t *cb = lv_event_get_current_target(e);
    const char *item_id = (const char *)lv_event_get_user_data(e);
    if (!item_id) return;

    bool new_state = lv_obj_has_state(cb, LV_STATE_CHECKED);

    /* 서버에 PATCH */
    bool ok = supabase_toggle_checklist_item(item_id, new_state);
    fprintf(stderr, "[checklist] item=%s new=%s patch=%s\n",
            item_id, new_state ? "true" : "false", ok ? "ok" : "FAIL");

    /* 로컬 카운트 갱신 */
    if (new_state) checked_count++;
    else           checked_count--;

    /* progress 라벨 갱신 */
    if (progress_label) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d/%d 완료", checked_count, total_count);
        lv_label_set_text(progress_label, buf);
    }
}

/* ── 화면 전환 타이머 콜백 ── */
static void goto_beforestudy_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    ui_screen_beforestudy_create(LV_SCR_LOAD_ANIM_NONE);
}

/* ── 조기종료 버튼 콜백 ── */
static void early_end_cb(lv_event_t *e)
{
    (void)e;

    if (tick_timer) { lv_timer_delete(tick_timer); tick_timer = NULL; }
    if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }

    /* ── 세션 로컬 저장 ── */
    local_session_t session;
    time_t end_time = time(NULL);

    int sec = (int)(end_time - study_start_time);
    if (sec < 0) sec = 0;

    snprintf(session.id, sizeof(session.id), "%d", (int)time(NULL));
    strncpy(session.subject_name, saved_subject_name, sizeof(session.subject_name) - 1);
    session.subject_name[sizeof(session.subject_name) - 1] = '\0';
    strncpy(session.subject_id, saved_subject_id, sizeof(session.subject_id) - 1);
    session.subject_id[sizeof(session.subject_id) - 1] = '\0';
    strncpy(session.color_hex, saved_color_hex, sizeof(session.color_hex) - 1);
    session.color_hex[sizeof(session.color_hex) - 1] = '\0';
    strftime(session.start_time, sizeof(session.start_time),
             "%Y-%m-%dT%H:%M:%S", localtime(&study_start_time));
    strftime(session.end_time, sizeof(session.end_time),
             "%Y-%m-%dT%H:%M:%S", localtime(&end_time));
    session.duration_seconds = sec;
    session.pushed = false;
    sessions_save(&session);

    time_label     = NULL;
    timer_label    = NULL;
    progress_label = NULL;

    /* studying 중 토글된 체크리스트가 beforestudy 에서도 보이도록 캐시 무효화.
     * 다음 beforestudy_create() 가 네트워크에서 최신 데이터를 다시 받아온다. */
    ui_screen_beforestudy_invalidate_cache();

    lv_timer_create(goto_beforestudy_cb, 50, NULL);
}

/* ── 화면 생성 ── */
void ui_screen_studying_create(const studying_info_t *info)
{
    scr_ref     = NULL;
    elapsed_sec = 0;
    checked_count = info->checklist_checked_count;
    total_count   = info->checklist_item_count;

    /* 세션 기록용 저장 */
    study_start_time = time(NULL);
    strncpy(saved_subject_name, info->subject_name, sizeof(saved_subject_name) - 1);
    saved_subject_name[sizeof(saved_subject_name) - 1] = '\0';
    strncpy(saved_subject_id, info->subject_id, sizeof(saved_subject_id) - 1);
    saved_subject_id[sizeof(saved_subject_id) - 1] = '\0';
    if (info->color_hex)
        strncpy(saved_color_hex, info->color_hex, sizeof(saved_color_hex) - 1);
    else
        strncpy(saved_color_hex, "#4CAF50", sizeof(saved_color_hex) - 1);
    saved_color_hex[sizeof(saved_color_hex) - 1] = '\0';

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ── 상단: 시계 (좌) + 경과 시간 (우) ── */
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

    char hms[16];
    format_hms(hms, sizeof(hms), elapsed_sec);
    timer_label = lv_label_create(top);
    lv_label_set_text(timer_label, hms);
    lv_obj_set_style_text_font(timer_label, &font_korean_20_bold, 0);
    lv_obj_set_style_text_color(timer_label, lv_color_hex(0x2196F3), 0);

    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(div1, 0, 0);

    /* ── 정보 영역: 과목 + 진행률 (컴팩트) ── */
    lv_obj_t *info_sec = lv_obj_create(scr);
    lv_obj_set_width(info_sec, SCREEN_W);
    lv_obj_set_height(info_sec, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(info_sec, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(info_sec, 0, 0);
    lv_obj_set_style_pad_top(info_sec, 6, 0);
    lv_obj_set_style_pad_bottom(info_sec, 4, 0);
    lv_obj_set_flex_flow(info_sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_sec, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(info_sec, 2, 0);

    /* 과목 아이콘 + 이름 */
    lv_obj_t *subj_row = lv_obj_create(info_sec);
    lv_obj_set_size(subj_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(subj_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(subj_row, 0, 0);
    lv_obj_set_style_pad_all(subj_row, 0, 0);
    lv_obj_set_flex_flow(subj_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(subj_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(subj_row, 6, 0);

    lv_obj_t *icon = lv_obj_create(subj_row);
    lv_obj_set_size(icon, 12, 12);
    lv_obj_set_style_bg_color(icon, info->color, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_radius(icon, 3, 0);

    lv_obj_t *subj_name = lv_label_create(subj_row);
    lv_label_set_text(subj_name, info->subject_name);
    lv_obj_set_style_text_font(subj_name, &font_korean_20, 0);

    /* "N/M 완료" */
    char prog_buf[24];
    snprintf(prog_buf, sizeof(prog_buf), "%d/%d 완료", checked_count, total_count);
    progress_label = lv_label_create(info_sec);
    lv_label_set_text(progress_label, prog_buf);
    lv_obj_set_style_text_font(progress_label, &font_korean_16, 0);
    lv_obj_set_style_text_color(progress_label, lv_color_hex(0x888888), 0);

    /* ── 구분선 ── */
    lv_obj_t *div1b = lv_obj_create(scr);
    lv_obj_set_size(div1b, SCREEN_W, 1);
    lv_obj_set_style_bg_color(div1b, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(div1b, 0, 0);

    /* ── 체크리스트 (스크롤 영역) ── */
    lv_obj_t *checklist_cont = lv_obj_create(scr);
    lv_obj_set_width(checklist_cont, SCREEN_W);
    lv_obj_set_flex_grow(checklist_cont, 1);
    lv_obj_set_style_bg_opa(checklist_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(checklist_cont, 0, 0);
    lv_obj_set_style_pad_top(checklist_cont, 6, 0);
    lv_obj_set_style_pad_bottom(checklist_cont, 4, 0);
    lv_obj_set_style_pad_left(checklist_cont, 8, 0);
    lv_obj_set_style_pad_right(checklist_cont, 8, 0);
    lv_obj_set_flex_flow(checklist_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(checklist_cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(checklist_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(checklist_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(checklist_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scroll_dir(checklist_cont, LV_DIR_VER);
    lv_obj_set_style_pad_row(checklist_cont, 6, 0);

    /* 체크박스 생성 (인터랙티브) */
    for (int i = 0; i < info->checklist_item_count; i++) {
        checklist_item_t *item = &info->checklist_items[i];

        lv_obj_t *cb = lv_checkbox_create(checklist_cont);
        lv_checkbox_set_text(cb, item->text);
        lv_obj_set_style_text_font(cb, &font_korean_16, 0);
        lv_obj_set_width(cb, SCREEN_W - 24);

        if (item->is_checked)
            lv_obj_add_state(cb, LV_STATE_CHECKED);

        /* user_data로 item->id 전달 (static 배열이므로 안전) */
        lv_obj_set_user_data(cb, (void *)item->id);
        lv_obj_add_event_cb(cb, checklist_item_click_cb,
                            LV_EVENT_CLICKED, NULL);
    }

    /* ── 하단: 컨트롤 버튼 ── */
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
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *end_btn = lv_btn_create(bottom);
    lv_obj_set_size(end_btn, (int)(SCREEN_W * 0.45), 48);
    lv_obj_set_style_bg_color(end_btn, lv_color_hex(0xFFCDD2), 0);
    lv_obj_set_style_bg_color(end_btn, lv_color_hex(0xEF9A9A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(end_btn, 8, 0);
    lv_obj_set_style_border_width(end_btn, 1, 0);
    lv_obj_set_style_border_color(end_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(end_btn, early_end_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *end_lbl = lv_label_create(end_btn);
    lv_label_set_text(end_lbl, "▶| 종료");
    lv_obj_set_style_text_font(end_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(end_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(end_lbl);

    clock_timer = lv_timer_create(update_time_cb, 1000, NULL);
    update_time_cb(NULL);

    tick_timer = lv_timer_create(tick_cb, 1000, NULL);

    scr_ref = scr;
    lv_screen_load(scr);
}
