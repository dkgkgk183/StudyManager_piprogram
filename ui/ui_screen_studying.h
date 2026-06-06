#ifndef UI_SCREEN_STUDYING_H
#define UI_SCREEN_STUDYING_H
#include "../lvgl/lvgl.h"
#include "../app/app_supabase.h"

typedef struct {
    const char *subject_name;
    const char *subject_id;
    const char *color_hex;   /* "#FF5722" 형태, NULL이면 "#4CAF50" 사용 */
    lv_color_t color;
    const char *nfc_id;      /* 현재 세션의 NFC UID (resume 시 매칭용) */
    /* 체크리스트 (체크 가능) */
    checklist_item_t *checklist_items;
    int checklist_item_count;
    int checklist_checked_count;
} studying_info_t;

void ui_screen_studying_create(const studying_info_t *info);
#endif
