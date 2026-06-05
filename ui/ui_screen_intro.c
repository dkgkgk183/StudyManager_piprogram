#include "ui_screen_intro.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "app/app_hx711.h"
#include "app/app_nfc.h"
#include "app/app_supabase.h"
#include "app/app_motor.h"
#include "ui_screen_device_select.h"
#include "ui_screen_study_plan.h"
#include "ui_screen_beforestudy.h"

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);
LV_FONT_DECLARE(lv_font_montserrat_32);

enum { STATE_IDLE, STATE_RECOGNIZING, STATE_WEIGHT_DONE, STATE_MOTOR_RUN, STATE_NFC_WAIT, STATE_NFC_DONE };

static lv_obj_t *status_label;
static lv_obj_t *guide_label;
static lv_obj_t *spinner;
static lv_obj_t *nfc_uid_label;
static lv_obj_t *master_btn_label;
static int state = STATE_IDLE;
static uint32_t recognize_start = 0;
static lv_timer_t *weight_timer;
static lv_timer_t *nfc_timer;
static bool master_running = false;

static volatile bool supabase_query_done = false;
static volatile bool supabase_found = false;
static char supabase_device_number[32] = {0};
static char supabase_nfc_id[32] = {0};

static void remove_spinner(void) {
    if (spinner) {
        lv_obj_delete(spinner);
        spinner = NULL;
    }
}

static void go_idle(void) {
    state = STATE_IDLE;
    if (!master_running) app_motor_stop();
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
    if (master_running) return;
    if (state == STATE_NFC_WAIT || state == STATE_NFC_DONE) return;
    if (!hx711_is_ready()) return;

    float weight = hx711_read_weight();
    if (weight < 0) weight = 0;

    switch (state) {
    case STATE_IDLE:
        if (weight >= 50) {
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
        if (weight < 20) {
            go_idle();
        } else if (lv_tick_elaps(recognize_start) >= 3000) {
            state = STATE_WEIGHT_DONE;
            remove_spinner();
            lv_label_set_text(status_label, "인식 완료!");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x2E7D32), 0);
            // 모터 시작 (forward, 1.5s 연속) — 무게 감지 후 닫기
            state = STATE_MOTOR_RUN;
            app_motor_run(true, 1350);
            lv_label_set_text(status_label, "모터 동작 중...");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
        }
        break;

    case STATE_MOTOR_RUN:
        if (!app_motor_run_active()) {
            // NFC 단계로 전환
            transition_to_nfc_cb(NULL);
        }
        break;

    default:
        break;
    }
}

static void *supabase_query_thread(void *arg) {
    (void)arg;
    char nfc_id[32];
    strncpy(nfc_id, supabase_nfc_id, sizeof(nfc_id) - 1);
    nfc_id[sizeof(nfc_id) - 1] = '\0';

    char device_number[32] = {0};
    bool found = supabase_find_device(nfc_id, device_number, sizeof(device_number));

    strncpy(supabase_device_number, device_number, sizeof(supabase_device_number) - 1);
    supabase_device_number[sizeof(supabase_device_number) - 1] = '\0';
    if (found) supabase_set_user_id(device_number);
    supabase_found = found;
    supabase_query_done = true;
    return NULL;
}

static void goto_beforestudy_after_delay_cb(lv_timer_t *timer) {
    /* 3초 대기 후 beforestudy 진입.
     * 화면 전환 전 intro 타이머를 명시적으로 정리해
     * beforestudy 화면이 떠있는 동안 dangling 콜백 방지. */
    if (weight_timer) { lv_timer_delete(weight_timer); weight_timer = NULL; }
    if (nfc_timer)    { lv_timer_delete(nfc_timer);    nfc_timer    = NULL; }
    supabase_clear_local_caches();
    ui_screen_beforestudy_invalidate_cache();
    ui_screen_beforestudy_create(LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

static void supabase_result_timer_cb(lv_timer_t *timer) {
    if (!supabase_query_done) return;
    lv_timer_delete(timer);

    if (supabase_found) {
        lv_label_set_text(status_label, "등록 완료!");

        /* 3초 후 beforestudy 로 이동 (사용자가 확인 메시지를 볼 시간 확보) */
        lv_timer_t *t = lv_timer_create(goto_beforestudy_after_delay_cb, 3000, NULL);
        lv_timer_set_repeat_count(t, 1);
    } else {
        /* device_select 화면으로 전환 전 intro 타이머 정리 */
        if (weight_timer) { lv_timer_delete(weight_timer); weight_timer = NULL; }
        if (nfc_timer)    { lv_timer_delete(nfc_timer);    nfc_timer    = NULL; }
        ui_screen_device_select_create(supabase_nfc_id);
    }
}

static void nfc_timer_cb(lv_timer_t *timer) {
    if (master_running) return;
    if (state != STATE_NFC_WAIT) return;

    if (g_nfc.tag_detected) {
        state = STATE_NFC_DONE;
        remove_spinner();
        lv_label_set_text(status_label, "NFC 인식 완료!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x2E7D32), 0);

        const char *uid = (const char *)g_nfc.uid_str;
        lv_timer_pause(weight_timer);
        lv_timer_pause(nfc_timer);

        strncpy(supabase_nfc_id, uid, sizeof(supabase_nfc_id) - 1);
        supabase_nfc_id[sizeof(supabase_nfc_id) - 1] = '\0';
        supabase_query_done = false;
        supabase_found = false;

        pthread_t tid;
        pthread_create(&tid, NULL, supabase_query_thread, NULL);
        pthread_detach(tid);

        lv_timer_create(supabase_result_timer_cb, 100, NULL);
    }
}

static void master_btn_cb(lv_event_t *e) {
    (void)e;
    if (!master_running) {
        app_motor_reverse();
        master_running = true;
        lv_label_set_text(master_btn_label, "STOP");
    } else {
        app_motor_stop();
        master_running = false;
        lv_label_set_text(master_btn_label, "OPEN");
    }
}

void create_study_manager_ui(void) {
    /* ── 이전 인스턴스 정리 ──
     * done → idle 복귀 후 다시 "공부 시작하기!" 가 눌리면
     * 정적 state 가 STATE_NFC_DONE 으로 남아있고
     * 이전 weight/nfc 타이머가 살아있는 채로 다음 cycle 이 시작됨.
     * 새 weight_timer_cb 가 첫 줄에서 return 해버려 무게 인식 실패. */
    if (weight_timer) { lv_timer_delete(weight_timer); weight_timer = NULL; }
    if (nfc_timer)    { lv_timer_delete(nfc_timer);    nfc_timer    = NULL; }

    state = STATE_IDLE;
    master_running = false;
    supabase_query_done = false;
    supabase_found = false;
    supabase_nfc_id[0] = '\0';
    supabase_device_number[0] = '\0';
    recognize_start = 0;

    /* 이전 화면이 delete 된 경우 dangling pointer 방지 */
    status_label = NULL;
    guide_label = NULL;
    spinner = NULL;
    nfc_uid_label = NULL;
    master_btn_label = NULL;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "STUDY\nMANAGER");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t *divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 160, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 115);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "대기 중...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(status_label, &font_korean_20, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 20);

    weight_timer = lv_timer_create(weight_timer_cb, 500, NULL);
    nfc_timer = lv_timer_create(nfc_timer_cb, 300, NULL);

    guide_label = lv_label_create(scr);
    lv_label_set_text(guide_label, "스마트폰을\n상자 위에 올려주세요");
    lv_obj_set_style_text_color(guide_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(guide_label, &font_korean_16, 0);
    lv_obj_set_style_text_align(guide_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(guide_label, LV_ALIGN_BOTTOM_MID, 0, -45);

    // 마스터 버튼 (OPEN/STOP)
    lv_obj_t *master_btn = lv_btn_create(scr);
    lv_obj_set_size(master_btn, 60, 30);
    lv_obj_align(master_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(master_btn, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_bg_color(master_btn, lv_color_hex(0xBF360C), LV_STATE_PRESSED);
    lv_obj_add_event_cb(master_btn, master_btn_cb, LV_EVENT_CLICKED, NULL);

    master_btn_label = lv_label_create(master_btn);
    lv_label_set_text(master_btn_label, "OPEN");
    lv_obj_center(master_btn_label);

    lv_screen_load(scr);
}

void ui_screen_intro_resume(void) {
    state = STATE_IDLE;
    master_running = false;
    supabase_query_done = false;
    supabase_found = false;
    if (weight_timer) lv_timer_resume(weight_timer);
    if (nfc_timer)    lv_timer_resume(nfc_timer);
}
