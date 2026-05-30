#include "ui_screen_calibrate.h"
#include "ui_screen_beforestudy.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_korean_16);
LV_FONT_DECLARE(font_korean_20);

#define SCREEN_W 240
#define SCREEN_H 320

/* ── 십자 위치 (4귀퉁이에서 약간 안쪽) ── */
#define CROSS_MARGIN 30

typedef struct {
    int16_t x;
    int16_t y;
} cross_t;

static const cross_t cross_pos[4] = {
    {CROSS_MARGIN,            CROSS_MARGIN},            /* 좌상 */
    {SCREEN_W - CROSS_MARGIN, CROSS_MARGIN},            /* 우상 */
    {CROSS_MARGIN,            SCREEN_H - CROSS_MARGIN},  /* 좌하 */
    {SCREEN_W - CROSS_MARGIN, SCREEN_H - CROSS_MARGIN},  /* 우하 */
};

static int current_step;           /* 0~3: 어떤 십자를 터치하는지 */
static int16_t raw_vals[4][2];     /* 각 십자에서 읽은 raw 값 */
static bool waiting_for_release;   /* 손 뗄 때까지 대기 */
static lv_obj_t *scr;
static lv_obj_t *step_label;
static lv_obj_t *cross_objs[4];
static lv_timer_t *touch_timer;

/* ── 십자 그리기 ── */
static lv_obj_t *create_cross(lv_obj_t *parent, int16_t cx, int16_t cy) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 24, 24);
    lv_obj_set_pos(cont, cx - 12, cy - 12);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* 가로선 */
    lv_obj_t *h = lv_obj_create(cont);
    lv_obj_set_size(h, 24, 2);
    lv_obj_set_style_bg_color(h, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(h, 0, 0);
    lv_obj_center(h);

    /* 세로선 */
    lv_obj_t *v = lv_obj_create(cont);
    lv_obj_set_size(v, 2, 24);
    lv_obj_set_style_bg_color(v, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(v, 0, 0);
    lv_obj_center(v);

    return cont;
}

/* ── 외부 raw 읽기 함수 (app_touch.c에 선언 필요) ── */
extern bool xpt2046_read_raw(int16_t *rx, int16_t *ry);

/* ── 터치 폴링 타이머 ── */
static void touch_poll_cb(lv_timer_t *timer) {
    (void)timer;
    int16_t rx, ry;
    bool touched = xpt2046_read_raw(&rx, &ry);

    /* 손을 뗄 때까지 대기 */
    if (waiting_for_release) {
        if (touched) return;       /* 아직 누르고 있음 */
        waiting_for_release = false;
        return;
    }

    if (!touched) return;          /* 터치 안 됨 */

    raw_vals[current_step][0] = rx;
    raw_vals[current_step][1] = ry;
    waiting_for_release = true;

    /* 현재 십자 완료 표시 */
    lv_obj_set_style_bg_color(cross_objs[current_step], lv_color_hex(0x4CAF50), 0);

    current_step++;
    if (current_step >= 4) {
        /* ── 캘리브레이션 계산 ── */
        int16_t x_min = (raw_vals[0][0] + raw_vals[2][0]) / 2;  /* 좌측 평균 */
        int16_t x_max = (raw_vals[1][0] + raw_vals[3][0]) / 2;  /* 우측 평균 */
        int16_t y_min = (raw_vals[0][1] + raw_vals[1][1]) / 2;  /* 상단 평균 */
        int16_t y_max = (raw_vals[2][1] + raw_vals[3][1]) / 2;  /* 하단 평균 */

        /* 결과 화면으로 전환 */
        lv_timer_delete(touch_timer);
        touch_timer = NULL;

        lv_obj_clean(scr);

        lv_obj_t *title = lv_label_create(scr);
        lv_label_set_text(title, "캘리브레이션 완료");
        lv_obj_set_style_text_font(title, &font_korean_20, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "X_MIN: %d\nX_MAX: %d\nY_MIN: %d\nY_MAX: %d",
                 x_min, x_max, y_min, y_max);
        lv_obj_t *info = lv_label_create(scr);
        lv_label_set_text(info, buf);
        lv_obj_set_style_text_font(info, &font_korean_16, 0);
        lv_obj_align(info, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t *hint = lv_label_create(scr);
        lv_label_set_text(hint, "app_touch.c의 값을\n위 숫자로 교체하세요");
        lv_obj_set_style_text_font(hint, &font_korean_16, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);

        return;
    }

    /* 다음 단계 안내 */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / 4 터치하세요", current_step + 1);
    lv_label_set_text(step_label, buf);

    /* 다음 십자 하이라이트 */
    lv_obj_set_style_bg_color(cross_objs[current_step], lv_color_hex(0xFF5722), 0);
}

void ui_screen_calibrate_create(void) {
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFAFAFA), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 안내 문구 */
    step_label = lv_label_create(scr);
    lv_label_set_text(step_label, "1 / 4 터치하세요");
    lv_obj_set_style_text_font(step_label, &font_korean_20, 0);
    lv_obj_set_style_text_color(step_label, lv_color_hex(0x222222), 0);
    lv_obj_align(step_label, LV_ALIGN_TOP_MID, 0, 10);

    /* 십자 4개 생성 */
    for (int i = 0; i < 4; i++) {
        cross_objs[i] = create_cross(scr, cross_pos[i].x, cross_pos[i].y);
    }

    /* 첫 번째 십자 하이라이트 */
    current_step = 0;
    waiting_for_release = false;
    lv_obj_set_style_bg_color(cross_objs[0], lv_color_hex(0xFF5722), 0);

    /* 터치 폴링 타이머 (50ms 간격) */
    touch_timer = lv_timer_create(touch_poll_cb, 50, NULL);

    lv_screen_load(scr);
}
