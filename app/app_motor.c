#include "app_motor.h"
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// BCM 핀 번호
#define PIN_IN1    19
#define PIN_IN2    26
#define PIN_IN3    16
#define PIN_IN4    20
#define PIN_ENA    12
#define PIN_ENB    13

static struct gpiod_chip *chip = NULL;
static struct gpiod_line *in1, *in2, *in3, *in4, *ena, *enb;

// 소프트웨어 PWM
static pthread_t pwm_thread;
static volatile bool pwm_running = false;
static volatile int pwm_duty = 60;  // 듀티 사이클 (0~100)

static void *pwm_fn(void *arg) {
    (void)arg;
    while (pwm_running) {
        int on_us = pwm_duty * 10;    // 1ms 주기 기준
        int off_us = (100 - pwm_duty) * 10;
        if (on_us > 0) {
            gpiod_line_set_value(ena, 1);
            gpiod_line_set_value(enb, 1);
            usleep(on_us);
        }
        if (off_us > 0) {
            gpiod_line_set_value(ena, 0);
            gpiod_line_set_value(enb, 0);
            usleep(off_us);
        }
    }
    gpiod_line_set_value(ena, 0);
    gpiod_line_set_value(enb, 0);
    return NULL;
}

bool app_motor_init(void) {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) return false;

    in1 = gpiod_chip_get_line(chip, PIN_IN1);
    in2 = gpiod_chip_get_line(chip, PIN_IN2);
    in3 = gpiod_chip_get_line(chip, PIN_IN3);
    in4 = gpiod_chip_get_line(chip, PIN_IN4);
    ena = gpiod_chip_get_line(chip, PIN_ENA);
    enb = gpiod_chip_get_line(chip, PIN_ENB);

    if (!in1 || !in2 || !in3 || !in4 || !ena || !enb) return false;

    gpiod_line_request_output(in1, "motor", 0);
    gpiod_line_request_output(in2, "motor", 0);
    gpiod_line_request_output(in3, "motor", 0);
    gpiod_line_request_output(in4, "motor", 0);
    gpiod_line_request_output(ena, "motor", 0);
    gpiod_line_request_output(enb, "motor", 0);

    return true;
}

void app_motor_forward(void) {
    gpiod_line_set_value(in1, 0);
    gpiod_line_set_value(in2, 1);
    gpiod_line_set_value(in3, 0);
    gpiod_line_set_value(in4, 1);

    if (!pwm_running) {
        pwm_running = true;
        pthread_create(&pwm_thread, NULL, pwm_fn, NULL);
    }
}

void app_motor_reverse(void) {
    gpiod_line_set_value(in1, 1);
    gpiod_line_set_value(in2, 0);
    gpiod_line_set_value(in3, 1);
    gpiod_line_set_value(in4, 0);

    if (!pwm_running) {
        pwm_running = true;
        pthread_create(&pwm_thread, NULL, pwm_fn, NULL);
    }
}

void app_motor_stop(void) {
    pwm_running = false;
    if (pwm_thread) {
        pthread_join(pwm_thread, NULL);
        pwm_thread = 0;
    }
    gpiod_line_set_value(in1, 0);
    gpiod_line_set_value(in2, 0);
    gpiod_line_set_value(in3, 0);
    gpiod_line_set_value(in4, 0);
}

// 연속 동작 모드: duration_ms 동안 모터를 한 방향으로 PWM 회전
static volatile bool run_active = false;

static void *run_fn(void *arg) {
    int duration_us = (intptr_t)arg;
    run_active = true;
    int elapsed = 0;
    while (run_active && elapsed < duration_us) {
        int on_us = pwm_duty * 10;
        int off_us = (100 - pwm_duty) * 10;
        int cycle = on_us + off_us;
        if (cycle <= 0) {
            usleep(1000);
            elapsed += 1000;
            continue;
        }
        if (on_us > 0) {
            gpiod_line_set_value(ena, 1);
            gpiod_line_set_value(enb, 1);
            usleep(on_us);
        }
        if (off_us > 0) {
            gpiod_line_set_value(ena, 0);
            gpiod_line_set_value(enb, 0);
            usleep(off_us);
        }
        elapsed += cycle;
    }
    gpiod_line_set_value(ena, 0);
    gpiod_line_set_value(enb, 0);
    run_active = false;
    return NULL;
}

void app_motor_run(bool forward, int duration_ms) {
    // 기존 모터 동작 중지
    if (pwm_running) {
        app_motor_stop();
    }
    if (run_active) {
        run_active = false;
        usleep(20000);
    }

    // 모터 OFF (안전)
    gpiod_line_set_value(ena, 0);
    gpiod_line_set_value(enb, 0);

    // 방향 설정
    if (forward) {
        gpiod_line_set_value(in1, 0);
        gpiod_line_set_value(in2, 1);
        gpiod_line_set_value(in3, 0);
        gpiod_line_set_value(in4, 1);
    } else {
        gpiod_line_set_value(in1, 1);
        gpiod_line_set_value(in2, 0);
        gpiod_line_set_value(in3, 1);
        gpiod_line_set_value(in4, 0);
    }

    // 모터 ON (PWM으로 duration_ms 동안 회전)
    pthread_t tid;
    pthread_create(&tid, NULL, run_fn, (void *)(intptr_t)(duration_ms * 1000));
    pthread_detach(tid);
}

bool app_motor_run_active(void) {
    return run_active;
}

void app_motor_set_speed(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    pwm_duty = percent;
}

void app_motor_cleanup(void) {
    app_motor_stop();
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}
