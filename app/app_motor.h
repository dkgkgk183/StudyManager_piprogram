#ifndef APP_MOTOR_H
#define APP_MOTOR_H

#include <stdbool.h>

bool app_motor_init(void);
void app_motor_forward(void);   // 양쪽 모터 정방향
void app_motor_reverse(void);   // 양쪽 모터 역방향
void app_motor_stop(void);      // 모터 정지
void app_motor_run(bool forward, int duration_ms);   // 연속 동작 (duration_ms 동안)
bool app_motor_run_active(void);                     // 연속 동작 중 여부
void app_motor_set_speed(int percent);  // 속도 조절 (0~100)
void app_motor_cleanup(void);

#endif
