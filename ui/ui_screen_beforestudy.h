#ifndef UI_SCREEN_BEFORESTUDY_H
#define UI_SCREEN_BEFORESTUDY_H
#include "../lvgl/lvgl.h"
void ui_screen_beforestudy_create(lv_scr_load_anim_t anim);

/* intro 등 외부에서 user_id 가 바뀐 경우 호출:
 *  다음 ui_screen_beforestudy_create() 가 캐시된 데이터가 아닌
 *  네트워크에서 새로 받아오도록 캐시 플래그를 무효화한다. */
void ui_screen_beforestudy_invalidate_cache(void);
#endif
