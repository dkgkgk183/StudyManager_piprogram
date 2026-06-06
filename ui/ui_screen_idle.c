#include "ui_screen_idle.h"
#include "ui_screen_intro.h"
#include "../app/app_motor.h"
#include "../app/app_sessions.h"
#include <stdio.h>
#include <time.h>

LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_timer_t *clock_timer;

static void update_clock(lv_timer_t *timer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(time_label, buf);

    const char *wday[] = {"일", "월", "화", "수", "목", "금", "토"};
    char dbuf[32];
    snprintf(dbuf, sizeof(dbuf), "%d월 %d일 %s요일",
             t->tm_mon + 1, t->tm_mday, wday[t->tm_wday]);
    lv_label_set_text(date_label, dbuf);
}

static void start_btn_cb(lv_event_t *e) {
    (void)e;
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    /* 서랍장 열기 (reverse, 1.3s 연속) */
    app_motor_run(false, 1000);
    create_study_manager_ui();
}

void ui_screen_idle_create(void) {
    /* idle 진입 = 이번 사용 사이클 종료. 모든 경로(공부완료/긴급종료/60초초과/
     * 조기종료)에서 세션은 이미 Supabase 로 push 되었으므로 로컬 사본은
     * 다음 사이클의 중복 409 업로드만 유발함. 깨끗이 비우고 시작. */
    sessions_clear_all();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // 상단 헤더
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 240, 70);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1B5E20), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "STUDY\nMANAGER");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &font_korean_20, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(title);

    // 시간
    time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);

    // 날짜
    date_label = lv_label_create(scr);
    lv_label_set_text(date_label, "1월 1일 월요일");
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(date_label, &font_korean_16, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 5);

    // 구분선
    lv_obj_t *divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 120, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 35);

    // 공부 시작하기 버튼
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 200, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1B5E20), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 22, 0);
    lv_obj_add_event_cb(btn, start_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "공부 시작하기!");
    lv_obj_set_style_text_font(btn_label, &font_korean_16, 0);
    lv_obj_center(btn_label);

    // 시계 타이머
    clock_timer = lv_timer_create(update_clock, 1000, NULL);
    update_clock(NULL);

    lv_screen_load(scr);
}
