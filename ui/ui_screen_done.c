#include "ui_screen_done.h"
#include "ui_screen_idle.h"
#include "../app/app_motor.h"
#include "../app/app_supabase.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);
LV_FONT_DECLARE(font_korean_20_bold);

#define SCREEN_W 240
#define SCREEN_H 320

static lv_obj_t   *countdown_label = NULL;
static lv_timer_t *countdown_timer = NULL;
static int         remaining_seconds;

static void update_countdown_label(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%d초 후 메인화면으로 복귀합니다.", remaining_seconds);
    lv_label_set_text(countdown_label, buf);
}

static void countdown_cb(lv_timer_t *timer)
{
    remaining_seconds--;
    if (remaining_seconds <= 0) {
        lv_timer_delete(timer);
        countdown_timer = NULL;
        countdown_label = NULL;
        /* idle 전환과 동시에 서랍장 닫기 (forward).
         * 최초 부팅은 main.c 가 직접 idle_create() 로 진입하므로
         * 여기를 거치지 않아 닫기 동작이 불필요하게 일어나지 않음. */
        app_motor_run(true, 1350);
        /* 다음 사용자는 처음부터 다시 NFC 인식이 필요하도록 세션 정리 */
        supabase_set_user_id(NULL);
        supabase_clear_local_caches();
        ui_screen_idle_create();
    } else {
        update_countdown_label();
    }
}

void ui_screen_done_create(int countdown_seconds)
{
    if (countdown_timer) { lv_timer_delete(countdown_timer); countdown_timer = NULL; }
    remaining_seconds = countdown_seconds;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);

    /* 상단 그린 밴드 (완료/축하 느낌) */
    lv_obj_t *top_band = lv_obj_create(scr);
    lv_obj_set_size(top_band, SCREEN_W, 6);
    lv_obj_set_style_bg_color(top_band, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_border_width(top_band, 0, 0);
    lv_obj_align(top_band, LV_ALIGN_TOP_MID, 0, 0);

    /* 메인 메시지 */
    lv_obj_t *main_label = lv_label_create(scr);
    lv_obj_set_style_text_font(main_label, &font_korean_20_bold, 0);
    lv_obj_set_style_text_color(main_label, lv_color_hex(0x222222), 0);
    lv_label_set_text(main_label, "오늘도 수고하셨습니다!");
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -35);

    /* 메인 메시지 아래 액센트 라인 */
    lv_obj_t *accent = lv_obj_create(scr);
    lv_obj_set_size(accent, 60, 3);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    lv_obj_align(accent, LV_ALIGN_CENTER, 0, -8);

    /* 카운트다운 */
    countdown_label = lv_label_create(scr);
    lv_obj_set_style_text_font(countdown_label, &font_korean_16, 0);
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0x888888), 0);
    lv_obj_set_width(countdown_label, SCREEN_W - 20);
    lv_obj_set_style_text_align(countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    update_countdown_label();
    lv_obj_align(countdown_label, LV_ALIGN_CENTER, 0, 25);

    countdown_timer = lv_timer_create(countdown_cb, 1000, NULL);

    /* 서랍장 열기 (reverse, 1.3s 연속) — 사용자가 물건을 가져갈 수 있도록 */
    app_motor_run(false, 1100);

    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, true);
}
