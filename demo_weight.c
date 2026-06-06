/*
 * demo_weight.c - PC/SDL2 무게 센서 테스트 데모
 *
 *   - 디스플레이: LVGL SDL 드라이버
 *   - 무게:       app/app_hx711_stub.c (항상 0 반환) — 실기에서 lvgl_app 사용 시 실측
 *   - 빌드:       cd build && make demo_weight && ./demo_weight
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/drivers/sdl/lv_sdl_keyboard.h"
#include "lvgl/drivers/sdl/lv_sdl_mousewheel.h"

#include "ui/ui_screen_weight_test.h"
#include "app/app_hx711.h"

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

    lv_display_t *disp = lv_sdl_window_create(240, 320);
    if (!disp) {
        fprintf(stderr, "SDL 윈도우 생성 실패\n");
        return 1;
    }
    lv_sdl_window_set_title(disp, "Weight Test (PC Demo)");
    lv_sdl_window_set_zoom(disp, 2.0f);

    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_sdl_mousewheel_create();

    hx711_init();

    ui_screen_weight_test_create();

    printf("\n=== Weight Test PC 데모 ===\n");
    printf("  - PC에서는 스텁이 0.0 g 만 표시\n");
    printf("  - 실기 검증은 build/lvgl_app 사용\n\n");

    while (1) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    hx711_close();
    return 0;
}
