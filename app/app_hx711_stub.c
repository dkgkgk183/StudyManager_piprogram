/*
 * app_hx711_stub.c - PC 데모용 무게센서 스텁
 *
 * HX711 GPIO 비트뱅잉 없이도 무게 API가 동작하도록
 * 항상 0g 를 반환한다.
 */

#include "app_hx711.h"

int32_t hx711_offset = 0;
float   hx711_scale  = 1.0f;

bool hx711_init(void)  { return true; }
void hx711_close(void) {}

bool    hx711_is_ready(void)   { return false; }
int32_t hx711_read_raw(void)   { return 0; }
float   hx711_read_weight(void) { return 0.0f; }
