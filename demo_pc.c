/*
 * demo_pc.c - PC/SDL2 데모 진입점
 *
 * 라즈베리파이 + ILI9341 + XPT2046 같은 실제 하드웨어 없이
 * PC의 SDL2 윈도우에서 동일 UI를 시연하기 위한 진입점.
 *
 *   - 디스플레이: LVGL SDL 드라이버 (lv_sdl_window_create)
 *   - 입력:       SDL 마우스/키보드/휠 (lv_sdl_mouse_create 등)
 *   - 하드웨어:   app/app_*_stub.c 의 no-op 스텁으로 대체
 *
 * 빌드:
 *   mkdir -p build && cd build
 *   cmake .. -DBUILD_PC_DEMO=ON
 *   make demo
 *   ./demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/drivers/sdl/lv_sdl_keyboard.h"
#include "lvgl/drivers/sdl/lv_sdl_mousewheel.h"

#include "ui/ui_screen_idle.h"
#include "app/app_nfc.h"
#include "app/app_hx711.h"
#include "app/app_motor.h"
#include "app/app_sessions.h"

static uint32_t get_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    lv_init();
    lv_tick_set_cb(get_tick_ms);

    /* ── SDL 디스플레이 (실제 ILI9341 대체) ─────────────────── */
    lv_display_t *disp = lv_sdl_window_create(240, 320);
    if (!disp) {
        fprintf(stderr, "SDL 윈도우 생성 실패\n");
        return 1;
    }
    lv_sdl_window_set_title(disp, "Study Manager (PC Demo)");

    /* 240x320 은 PC에서 너무 작으므로 2배 확대 */
    lv_sdl_window_set_zoom(disp, 2.0f);

    /* ── SDL 입력 디바이스 (XPT2046 터치 대체) ───────────────── */
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_sdl_mousewheel_create();

    /* ── 하드웨어 스텁 초기화 (실제 호출은 안전) ─────────────── */
    app_nfc_init();
    app_nfc_start();
    hx711_init();
    app_motor_init();
    sessions_init();

    /* ── UI 진입점: idle 화면부터 시작 ───────────────────────── */
    ui_screen_idle_create();

    printf("\n=== Study Manager PC 데모 ===\n");
    printf("  - 마우스로 터치 동작\n");
    printf("  - ESC 또는 창 닫기로 종료\n\n");

    while (1) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    app_motor_cleanup();
    app_nfc_stop();
    hx711_close();
    return 0;
}
