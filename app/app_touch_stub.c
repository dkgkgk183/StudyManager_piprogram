/*
 * app_touch_stub.c - PC 데모용 터치 스텁
 *
 * PC 데모에서는 LVGL SDL 드라이버가 마우스를 자동으로
 * pointer 입력 디바이스로 제공하므로, app_touch_* 는 호출되지
 * 않지만 컴파일 링크를 위해 빈 구현을 둔다.
 */

#include "app_touch.h"
#include <stddef.h>

void app_touch_init(void) {}

void app_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    if (data) {
        data->state   = LV_INDEV_STATE_RELEASED;
        data->point.x = 0;
        data->point.y = 0;
    }
}

bool xpt2046_read_raw(int16_t *rx, int16_t *ry) {
    if (rx) *rx = 0;
    if (ry) *ry = 0;
    return false;
}
