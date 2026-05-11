#ifndef APP_HX711_H
#define APP_HX711_H

#include <stdint.h>
#include <stdbool.h>

// GPIO 핀 번호
#define HX711_DT_PIN  5
#define HX711_SCK_PIN 6

// 보정 값
extern int32_t hx711_offset;
extern float hx711_scale;  // g/unit

// HX711 초기화
bool hx711_init(void);

// HX711 종료
void hx711_close(void);

// 원시 값 읽기 (24비트)
int32_t hx711_read_raw(void);

// 무게 읽기 (g)
float hx711_read_weight(void);

// 데이터 준비되었는지 확인
bool hx711_is_ready(void);

#endif
