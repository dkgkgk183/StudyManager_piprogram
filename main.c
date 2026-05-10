#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "ili9341.h"
#include "ui/ui_tab_nfc.h"
#include "ui/ui_tab_weight.h"
#include "ui/ui_tab_camera.h"
#include "app/app_nfc.h"
#include "app/app_touch.h"   // ← 추가

static uint32_t get_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void create_ui(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D0D1A), 0);

    lv_obj_t *tv = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 40);
    lv_obj_set_size(tv, 240, 320);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x0D0D1A), 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(tab_bar, 0, 0);

    lv_obj_t *tab1 = lv_tabview_add_tab(tv, "NFC");
    lv_obj_t *tab2 = lv_tabview_add_tab(tv, "무게");
    lv_obj_t *tab3 = lv_tabview_add_tab(tv, "카메라");

    ui_tab_nfc_create(tab1);
    ui_tab_weight_create(tab2);
    ui_tab_camera_create(tab3);
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

    create_ui();

    printf("루프 시작\n");
    while (1) {
        lv_timer_handler();
        usleep(5000);
    }

    app_nfc_stop();
    return 0;
}