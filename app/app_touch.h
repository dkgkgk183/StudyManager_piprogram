#ifndef APP_TOUCH_H
#define APP_TOUCH_H

#include "../lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void app_touch_init(void);
void app_touch_read(lv_indev_t *indev, lv_indev_data_t *data);
bool xpt2046_read_raw(int16_t *rx, int16_t *ry);

#endif