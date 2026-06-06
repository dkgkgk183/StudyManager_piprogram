/*
 * app_motor_stub.c - PC 데모용 모터 스텁
 *
 * GPIO/PWM 없이도 모터 API 호출이 안전하도록 모두 no-op.
 */

#include "app_motor.h"

bool app_motor_init(void)     { return true; }
void app_motor_cleanup(void)  {}

void app_motor_forward(void)  {}
void app_motor_reverse(void)  {}
void app_motor_stop(void)     {}

void app_motor_run(bool forward, int duration_ms) {
    (void)forward;
    (void)duration_ms;
}

bool app_motor_run_active(void)       { return false; }
void app_motor_set_speed(int percent) { (void)percent; }
