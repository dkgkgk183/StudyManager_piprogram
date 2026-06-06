#include "ui_screen_studying.h"
#include "ui_screen_beforestudy.h"
#include "ui_screen_idle.h"
#include "../app/app_sessions.h"
#include "../app/app_supabase.h"
#include "../app/app_motor.h"
#include "../app/app_nfc.h"
#include "../app/app_hx711.h"
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

/* 무게 감지 임계값 (intro 화면과 동일 정책) */
#define WEIGHT_LIFT_G        20.0f   /* 이 값 미만이면 폰 들림 */
#define WEIGHT_PLACE_G       50.0f   /* 이 값 이상이면 폰 놓임 */
#define PAUSE_TIMEOUT_MS     60000   /* 일시정지 카운트다운 60초 */

/* ── pause popup 상태 ── */
enum {
    PAUSE_OFF = 0,
    PAUSE_WAIT_LIFT,         /* (열림) 클릭 직후, 무게 0g 대기 (10초 grace) */
    PAUSE_COUNTING,          /* 무게 0g 감지 → 60초 카운트다운 진행 중 */
    PAUSE_SETTLE,            /* 폰이 다시 놓임 → 3초 settle 대기 (카운트다운 정지) */
    EMERGENCY_WAIT_LIFT,     /* 긴급종료 클릭 → 무게 들어올림 대기 */
};

/* ── 화면 상태 ── */
static lv_obj_t   *scr_ref;
static lv_obj_t   *time_label;
static lv_obj_t   *timer_label;
static lv_obj_t   *progress_label;
static lv_obj_t   *bottom_row;           /* (열림)/(종료) 버튼 영역 — pause 시 숨김 */
static lv_timer_t *tick_timer;
static lv_timer_t *clock_timer;

/* ── pause popup 위젯 ── */
static lv_obj_t   *pause_overlay;
static lv_obj_t   *pause_msg_label;
static lv_obj_t   *pause_countdown_label;
static lv_obj_t   *pause_emergency_btn;
static lv_timer_t *pause_check_timer;
static int         pause_state = PAUSE_OFF;
static uint32_t    pause_start_tick;       /* PAUSE_WAIT_LIFT 진입 시점 (10초 grace) */
static uint32_t    counting_start_tick;    /* PAUSE_COUNTING 진입 시점 (60초 카운트다운; settle 동안 정지) */
static uint32_t    settle_start_tick;      /* PAUSE_SETTLE 진입 시점 (3초 settle) */

/* ── 세션 메타 ── */
static int         elapsed_sec;
static int         checked_count;
static int         total_count;
static int         tray_open_count;
static time_t      study_start_time;
static char        saved_subject_name[64];
static char        saved_subject_id[40];
static char        saved_color_hex[8];
static char        saved_nfc_id[40];       /* resume 시 매칭할 NFC UID */

/* ── 시계 업데이트 ── */
static void update_time_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    if (time_label) lv_label_set_text(time_label, buf);
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
    if (timer_label) lv_label_set_text(timer_label, buf);
}

/* ── 체크박스 클릭 → Supabase PATCH + 진행 갱신 ── */
static void checklist_item_click_cb(lv_event_t *e)
{
    lv_obj_t *cb = lv_event_get_current_target(e);
    const char *item_id = (const char *)lv_obj_get_user_data(cb);
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

/* ── forward decls ── */
static void cleanup_pause_overlay(void);
static void save_and_end_session(int penalty, bool push_to_supabase, bool go_to_idle);

/* ── pause popup 갱신 헬퍼 ── */
static void set_pause_message(const char *text)
{
    if (pause_msg_label) lv_label_set_text(pause_msg_label, text);
}

static void set_pause_countdown_visible(bool visible)
{
    (void)visible;  /* 카운트다운 라벨은 항상 노출 — 두 상태 모두에서 사용 */
}

static void set_pause_emergency_visible(bool visible)
{
    if (!pause_emergency_btn) return;
    if (visible) lv_obj_clear_flag(pause_emergency_btn, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(pause_emergency_btn, LV_OBJ_FLAG_HIDDEN);
}

/* ── resume: 폰이 돌아왔고 NFC ID도 일치 → 서랍 닫고 공부 재개 ── */
static void resume_study_cb(lv_timer_t *timer)
{
    (void)timer;
    /* 서랍 닫기 (forward) — 시작/종료 시 닫기와 동일 duration */
    app_motor_run(true, 1275);
    cleanup_pause_overlay();
    if (bottom_row) lv_obj_clear_flag(bottom_row, LV_OBJ_FLAG_HIDDEN);
    if (tick_timer) lv_timer_resume(tick_timer);
}

/* ── 카운트다운 라벨 갱신 ── */
static void update_pause_countdown_label(void)
{
    if (!pause_countdown_label) return;

    if (pause_state == PAUSE_WAIT_LIFT) {
        /* 10초 자동 닫기 카운트다운 (10→0). 텍스트 안에 숫자 포함 */
        uint32_t elapsed = lv_tick_elaps(pause_start_tick);
        uint32_t remaining_ms = (elapsed < 10000) ? (10000 - elapsed) : 0;
        uint32_t remaining_s = (remaining_ms + 999) / 1000;
        if (remaining_s > 10) remaining_s = 10;
        char buf[48];
        snprintf(buf, sizeof(buf), "%u초 후 서랍장이\n다시 닫힙니다...", (unsigned)remaining_s);
        lv_label_set_text(pause_countdown_label, buf);
        lv_obj_set_style_text_color(pause_countdown_label, lv_color_hex(0xE65100), 0);
    } else if (pause_state == PAUSE_COUNTING || pause_state == PAUSE_SETTLE) {
        /* 60초 패널티 카운트다운 — settle 동안에는 counting_start_tick 가
         * 변하지 않으므로 자연스럽게 freeze 되어 표시됨 */
        uint32_t elapsed = lv_tick_elaps(counting_start_tick);
        uint32_t remaining_ms = (elapsed < PAUSE_TIMEOUT_MS) ? (PAUSE_TIMEOUT_MS - elapsed) : 0;
        uint32_t remaining_s = (remaining_ms + 999) / 1000;
        char buf[16];
        snprintf(buf, sizeof(buf), "%u초", (unsigned)remaining_s);
        lv_label_set_text(pause_countdown_label, buf);
        /* settle 중에는 색을 좀 더 차분하게 */
        lv_color_t c = (pause_state == PAUSE_SETTLE)
            ? lv_color_hex(0x9E9E9E)
            : lv_color_hex(0xD32F2F);
        lv_obj_set_style_text_color(pause_countdown_label, c, 0);
    } else {
        lv_label_set_text(pause_countdown_label, "");
    }
}

/* ── pause_check_timer: 무게/NFC 모니터링 (300ms) ── */
static void pause_check_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (pause_state == PAUSE_OFF) return;

    float weight = 0.0f;
    if (hx711_is_ready()) {
        weight = hx711_read_weight();
        if (weight < 0) weight = 0;
    }

    switch (pause_state) {
    case PAUSE_WAIT_LIFT:
        update_pause_countdown_label();
        if (weight < WEIGHT_LIFT_G) {
            /* 폰 들어올림 감지 → 60초 카운트다운 시작, 긴급종료 버튼 노출 */
            pause_state = PAUSE_COUNTING;
            counting_start_tick = lv_tick_get();
            set_pause_message("스마트폰을 다시\n상자 위에 올려주세요");
            set_pause_emergency_visible(true);
            update_pause_countdown_label();
        } else if (lv_tick_elaps(pause_start_tick) >= 10000) {
            /* 10초 대기 중 무게 변화 없음 → 자동 닫기 (트레이 카운트만 유지) */
            lv_timer_t *t = lv_timer_create(resume_study_cb, 50, NULL);
            lv_timer_set_repeat_count(t, 1);
        }
        break;

    case PAUSE_COUNTING: {
        update_pause_countdown_label();
        /* 폰 되돌아옴: 무게 + NFC ID 매칭 → 즉시 닫지 않고 3초 settle 으로 진입 */
        bool nfc_match = g_nfc.tag_detected && g_nfc.uid_str[0] != '\0';
        if (saved_nfc_id[0] != '\0') {
            nfc_match = nfc_match && (strncmp((const char *)g_nfc.uid_str,
                                              saved_nfc_id,
                                              sizeof(saved_nfc_id) - 1) == 0);
        }
        if (weight >= WEIGHT_PLACE_G && nfc_match) {
            pause_state = PAUSE_SETTLE;
            settle_start_tick = lv_tick_get();
            set_pause_message("확인 중...");
        } else if (lv_tick_elaps(counting_start_tick) >= PAUSE_TIMEOUT_MS) {
            /* 60초 경과 → 서랍 닫고, 패널티 1회, push, idle 로 */
            app_motor_run(true, 1275);
            save_and_end_session(1, true, true);
        }
        break;
    }

    case PAUSE_SETTLE: {
        /* 카운트다운은 freeze — update_pause_countdown_label 은 색만 갱신 */
        update_pause_countdown_label();
        bool nfc_match = g_nfc.tag_detected && g_nfc.uid_str[0] != '\0';
        if (saved_nfc_id[0] != '\0') {
            nfc_match = nfc_match && (strncmp((const char *)g_nfc.uid_str,
                                              saved_nfc_id,
                                              sizeof(saved_nfc_id) - 1) == 0);
        }
        if (weight < WEIGHT_PLACE_G || !nfc_match) {
            /* settle 중 폰이 다시 들림 → 카운트다운 재개
             * counting_start_tick 는 그대로 두면 lv_tick_elaps 가 자동으로
             * 멈춘 지점부터 이어서 카운트다운을 표시함 */
            pause_state = PAUSE_COUNTING;
            set_pause_message("스마트폰을 다시\n상자 위에 올려주세요");
        } else if (lv_tick_elaps(settle_start_tick) >= 3000) {
            /* 3초 settle 완료 → resume */
            lv_timer_t *t = lv_timer_create(resume_study_cb, 50, NULL);
            lv_timer_set_repeat_count(t, 1);
        }
        /* settle 중에도 60초 총 시간은 흘러가지만 counting_start_tick 기준
         * 이라 실제 deadline 은 settle 시간만큼 자연스럽게 연장됨. */
        else if (lv_tick_elaps(counting_start_tick) >= PAUSE_TIMEOUT_MS) {
            app_motor_run(true, 1275);
            save_and_end_session(1, true, true);
        }
        break;
    }

    case EMERGENCY_WAIT_LIFT:
        if (weight < WEIGHT_LIFT_G) {
            /* 폰 들어올림 감지 → 서랍 닫고 긴급 종료 */
            app_motor_run(true, 1275);
            save_and_end_session(0, true, true);
        }
        break;

    default:
        break;
    }
}

/* ── pause popup 생성 (서랍 열림 직후 호출) ──
 * records.c 팝업 구조 차용: 화면 flex 레이아웃에서 제외된 풀스크린 overlay
 * 위에 흰색 카드를 flex column 으로 올리고 메시지/카운트다운/버튼을 배치.
 * 이렇게 해야 scr 의 column flex 가 overlay 를 아래로 밀어내지 않음. */
static void create_pause_overlay(void)
{
    pause_state = PAUSE_WAIT_LIFT;
    pause_start_tick = lv_tick_get();  /* 10초 자동 닫기 + 이후 PAUSE_COUNTING 의 기준 */

    /* 풀스크린 반투명 배경 (스크린 flex 무시, 최상위 z-order) */
    pause_overlay = lv_obj_create(scr_ref);
    lv_obj_add_flag(pause_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(pause_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(pause_overlay, 0, 0);
    lv_obj_set_style_bg_color(pause_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pause_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(pause_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(pause_overlay);

    /* 중앙 카드 (records.c 와 동일 규격) */
    lv_obj_t *card = lv_obj_create(pause_overlay);
    lv_obj_set_size(card, 220, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, 170, 0);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 14, 0);

    /* 안내 메시지 (card 상단) */
    pause_msg_label = lv_label_create(card);
    lv_label_set_text(pause_msg_label, "스마트폰을\n들어 올려주세요");
    lv_obj_set_style_text_color(pause_msg_label, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(pause_msg_label, &font_korean_20, 0);
    lv_obj_set_style_text_align(pause_msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(pause_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(pause_msg_label, lv_pct(100));

    /* 카운트다운 (PAUSE_WAIT_LIFT 10초 + PAUSE_COUNTING 60초 모두 표시) */
    pause_countdown_label = lv_label_create(card);
    lv_label_set_text(pause_countdown_label, "");
    lv_obj_set_style_text_color(pause_countdown_label, lv_color_hex(0xE65100), 0);
    lv_obj_set_style_text_font(pause_countdown_label, &font_korean_20_bold, 0);
    lv_obj_set_style_text_align(pause_countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(pause_countdown_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(pause_countdown_label, lv_pct(100));

    /* 긴급 종료 버튼 (EMERGENCY_WAIT_LIFT 진입 시 숨김) */
    pause_emergency_btn = lv_btn_create(card);
    lv_obj_set_size(pause_emergency_btn, 140, 36);
    lv_obj_set_style_bg_color(pause_emergency_btn, lv_color_hex(0xFFCDD2), 0);
    lv_obj_set_style_bg_color(pause_emergency_btn, lv_color_hex(0xEF9A9A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(pause_emergency_btn, 8, 0);
    lv_obj_set_style_border_width(pause_emergency_btn, 1, 0);
    lv_obj_set_style_border_color(pause_emergency_btn, lv_color_hex(0x222222), 0);

    lv_obj_t *eb_lbl = lv_label_create(pause_emergency_btn);
    lv_label_set_text(eb_lbl, "긴급 종료");
    lv_obj_set_style_text_font(eb_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(eb_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(eb_lbl);

    /* 콜백은 forward decl 로 등록 (아래 정의) */
    extern void pause_emergency_btn_cb(lv_event_t *e);
    lv_obj_add_event_cb(pause_emergency_btn, pause_emergency_btn_cb, LV_EVENT_CLICKED, NULL);
    /* PAUSE_WAIT_LIFT 에서는 긴급종료 버튼 숨김 — 폰 들어올린 뒤에 노출 */
    lv_obj_add_flag(pause_emergency_btn, LV_OBJ_FLAG_HIDDEN);

    /* 초기 카운트다운 텍스트 (10초 자동 닫기) */
    update_pause_countdown_label();

    /* 무게 + NFC 모니터링 타이머 시작 */
    pause_check_timer = lv_timer_create(pause_check_timer_cb, 300, NULL);
}

/* ── pause popup 정리 ── */
static void cleanup_pause_overlay(void)
{
    if (pause_check_timer) { lv_timer_delete(pause_check_timer); pause_check_timer = NULL; }
    if (pause_overlay)     { lv_obj_delete(pause_overlay);     pause_overlay = NULL; }
    pause_msg_label = NULL;
    pause_countdown_label = NULL;
    pause_emergency_btn = NULL;
    pause_state = PAUSE_OFF;
    pause_start_tick = 0;
    counting_start_tick = 0;
    settle_start_tick = 0;
}

/* ── (열림) 버튼 콜백 ── */
static void open_btn_cb(lv_event_t *e)
{
    (void)e;
    if (pause_state != PAUSE_OFF) return;  /* 이미 pause 중이면 무시 */

    tray_open_count++;
    fprintf(stderr, "[studying] tray opened (count=%d)\n", tray_open_count);

    /* stopwatch 정지 */
    if (tick_timer) lv_timer_pause(tick_timer);

    /* 서랍 열기 (reverse, 1.0s) */
    app_motor_run(false, 1000);

    /* 하단 버튼 영역 숨김 — (열림) 재클릭/조기종료 차단 */
    if (bottom_row) lv_obj_add_flag(bottom_row, LV_OBJ_FLAG_HIDDEN);

    /* 팝업 생성 */
    create_pause_overlay();
}

/* ── 긴급 종료 버튼 콜백 (forward decl 만족) ── */
void pause_emergency_btn_cb(lv_event_t *e)
{
    (void)e;
    if (pause_state == PAUSE_OFF) return;

    pause_state = EMERGENCY_WAIT_LIFT;
    set_pause_message("스마트폰을 들어 올리세요!\n그럼 이후에 종료가 시작됩니다");
    set_pause_countdown_visible(false);
    set_pause_emergency_visible(false);  /* 재클릭/취소 차단 */
}

/* ── 화면 전환 타이머 콜백 ── */
static void goto_beforestudy_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    ui_screen_beforestudy_create(LV_SCR_LOAD_ANIM_NONE);
}

static void goto_idle_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    supabase_clear_local_caches();
    ui_screen_idle_create();
}

/* ── 세션 저장 + 종료 통합 ──
 * penalty: 0=없음(긴급종료/조기종료), 1=있음(타임아웃)
 * push_to_supabase: true면 Supabase study_sessions 로 즉시 업로드 시도
 * go_to_idle: true면 idle, false면 beforestudy 로 복귀 */
static void save_and_end_session(int penalty, bool push_to_supabase, bool go_to_idle)
{
    /* 팝업/타이머 정리 */
    cleanup_pause_overlay();
    if (tick_timer)  { lv_timer_delete(tick_timer);  tick_timer  = NULL; }
    if (clock_timer) { lv_timer_delete(clock_timer); clock_timer = NULL; }

    /* 세션 데이터 작성 — duration 은 실제 stopwatch 값 사용 (정지시간 제외) */
    local_session_t session;
    time_t end_time = time(NULL);

    snprintf(session.id, sizeof(session.id), "%d", (int)end_time);
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
    session.duration_seconds = elapsed_sec;
    session.tray_open_count   = tray_open_count;
    session.penalty_count     = penalty;
    session.pushed            = false;

    fprintf(stderr, "[studying] session end duration=%ds tray_open=%d penalty=%d push=%d\n",
            elapsed_sec, tray_open_count, penalty, push_to_supabase ? 1 : 0);

    /* UI dangling 방지 */
    time_label     = NULL;
    timer_label    = NULL;
    progress_label = NULL;
    bottom_row     = NULL;

    /* Supabase 업로드 먼저 — 결과를 보고 pushed 플래그 결정 후
     * sessions_save 로 in-memory/파일에 정확히 한 번 기록.
     * 기존 버그: sessions_save → upload → pushed=true 갱신 순서라
     *            로컬에는 항상 pushed=false 로 남아 다음 push 시도 시 409 발생. */
    if (push_to_supabase) {
        bool ok = supabase_upload_session(&session);
        fprintf(stderr, "[studying] supabase_upload_session -> %s\n", ok ? "ok" : "FAIL");
        if (ok) session.pushed = true;
    }
    sessions_save(&session);

    ui_screen_beforestudy_invalidate_cache();

    if (go_to_idle) {
        lv_timer_create(goto_idle_cb, 50, NULL);
    } else {
        lv_timer_create(goto_beforestudy_cb, 50, NULL);
    }
}

/* ── 조기종료(▶| 종료) 버튼 콜백 ── */
static void early_end_cb(lv_event_t *e)
{
    (void)e;
    if (pause_state != PAUSE_OFF) return;  /* pause 중에는 종료 버튼이 hidden 이지만 방어 */

    /* 일반 조기종료: penalty 없음, Supabase push 안 함 (records 화면에서 일괄 push) */
    save_and_end_session(0, false, false);
}

/* ── 화면 생성 ── */
void ui_screen_studying_create(const studying_info_t *info)
{
    scr_ref         = NULL;
    elapsed_sec     = 0;
    checked_count   = info->checklist_checked_count;
    total_count     = info->checklist_item_count;
    tray_open_count = 0;

    /* pause 상태 초기화 */
    pause_overlay          = NULL;
    pause_msg_label        = NULL;
    pause_countdown_label  = NULL;
    pause_emergency_btn    = NULL;
    pause_check_timer      = NULL;
    pause_state            = PAUSE_OFF;
    pause_start_tick       = 0;
    counting_start_tick    = 0;
    settle_start_tick      = 0;

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
    if (info->nfc_id)
        strncpy(saved_nfc_id, info->nfc_id, sizeof(saved_nfc_id) - 1);
    else
        saved_nfc_id[0] = '\0';
    saved_nfc_id[sizeof(saved_nfc_id) - 1] = '\0';

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

    bottom_row = lv_obj_create(scr);
    lv_obj_set_width(bottom_row, SCREEN_W);
    lv_obj_set_height(bottom_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bottom_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_row, 0, 0);
    lv_obj_set_style_pad_top(bottom_row, 6, 0);
    lv_obj_set_style_pad_bottom(bottom_row, 8, 0);
    lv_obj_set_style_pad_left(bottom_row, 10, 0);
    lv_obj_set_style_pad_right(bottom_row, 10, 0);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bottom_row, 8, 0);

    /* (열림) 버튼 — 서랍을 열어 일시정지 */
    lv_obj_t *open_btn = lv_btn_create(bottom_row);
    lv_obj_set_size(open_btn, 60, 48);
    lv_obj_set_style_bg_color(open_btn, lv_color_hex(0xFFE0B2), 0);
    lv_obj_set_style_bg_color(open_btn, lv_color_hex(0xFFCC80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(open_btn, 8, 0);
    lv_obj_set_style_border_width(open_btn, 1, 0);
    lv_obj_set_style_border_color(open_btn, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(open_btn, open_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *open_lbl = lv_label_create(open_btn);
    lv_label_set_text(open_lbl, "(열림)");
    lv_obj_set_style_text_font(open_lbl, &font_korean_16, 0);
    lv_obj_set_style_text_color(open_lbl, lv_color_hex(0x222222), 0);
    lv_obj_center(open_lbl);

    /* 종료 버튼 */
    lv_obj_t *end_btn = lv_btn_create(bottom_row);
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
