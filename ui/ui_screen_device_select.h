#ifndef UI_SCREEN_DEVICE_SELECT_H
#define UI_SCREEN_DEVICE_SELECT_H

#include "../lvgl/lvgl.h"

// nfc_id를 받아서 기기 선택 화면 표시
void ui_screen_device_select_create(const char *nfc_id);

#endif
