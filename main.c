#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "ili9341.h"
#include "ui/ui_screen_intro.h"
#include "app/app_nfc.h"
#include "app/app_touch.h"
#include "app/app_hx711.h"   // ← HX711 추가

LV_FONT_DECLARE(font_korean_16);

static uint32_t get_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(void) {
    printf("시작\n");
    lv_init();
    lv_tick_set_cb(get_tick_ms);
    ili9341_init();

    static uint8_t buf[ILI9341_WIDTH * 32 * 2];
    lv_display_t *disp = lv_display_create(ILI9341_WIDTH, ILI9341_HEIGHT);
    lv_display_set_flush_cb(disp, ili9341_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 터치 입력 장치 등록
    app_touch_init();
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, app_touch_read);

    app_nfc_init();
    app_nfc_start();

    // HX711 초기화
    hx711_init();

    create_study_manager_ui();

    printf("루프 시작\n");
    while (1) {
        lv_timer_handler();
        usleep(5000);
    }

    app_nfc_stop();
    hx711_close();
    return 0;
}