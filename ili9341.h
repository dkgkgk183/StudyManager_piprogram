#ifndef ILI9341_H
#define ILI9341_H

#include "lvgl/lvgl.h"

#define ILI9341_WIDTH   240
#define ILI9341_HEIGHT  320

#define GPIO_DC    24   // 물리 핀 18
#define GPIO_RESET 25   // 물리 핀 22

void ili9341_init(void);
void ili9341_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

#endif