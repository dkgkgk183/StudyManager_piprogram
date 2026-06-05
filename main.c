#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "ili9341.h"
#include "ui/ui_screen_idle.h"
#include "ui/ui_screen_calibrate.h"
#include "ui/ui_screen_nfc_test.h"
#include "app/app_nfc.h"
#include "app/app_touch.h"
#include "app/app_hx711.h"
#include "app/app_motor.h"
#include "app/app_sessions.h"

static uint32_t get_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(void) {
    lv_init();
    lv_tick_set_cb(get_tick_ms);
    ili9341_init();

    static uint8_t buf[ILI9341_WIDTH * 32 * 2];
    lv_display_t *disp = lv_display_create(ILI9341_WIDTH, ILI9341_HEIGHT);
    lv_display_set_flush_cb(disp, ili9341_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    app_touch_init();
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, app_touch_read);
    lv_indev_set_scroll_limit(indev, 30);  /* 저항식 터치: 스크롤 임계값 높임 */

    app_nfc_init();
    app_nfc_start();
    hx711_init();
    app_motor_init();
    sessions_init();

    /* 통합 진입점: idle → "공부 시작하기!" → intro(NFC) → beforestudy */
    ui_screen_idle_create();

    while (1) {
        lv_timer_handler();
        usleep(5000);
    }

    app_motor_cleanup();
    app_nfc_stop();
    hx711_close();
    return 0;
}
