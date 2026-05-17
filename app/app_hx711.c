#define _POSIX_C_SOURCE 199309L
#include "app_hx711.h"
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

static struct gpiod_line *dt_line = NULL;
static struct gpiod_line *sck_line = NULL;
static struct gpiod_chip *chip = NULL;

// 보정 값
int32_t hx711_offset = 183300;
float hx711_scale = 0.0003161f;

// 짧은 딜레이 (microsecond 단위, busy-wait)
static void delay_us(int us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

// 매우 짧은 딜레이용 nop 기반 busy-wait (us < 100일 때)
static void delay_short_us(int us) {
    // RPi 4 @ 1.5GHz 기준: 대략 1us당 1500번 루프
    volatile long count = us * 1500;
    while (count--) {}
}

bool hx711_init(void) {
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        fprintf(stderr, "HX711: gpiochip0 열기 실패\n");
        return false;
    }

    dt_line = gpiod_chip_get_line(chip, HX711_DT_PIN);
    sck_line = gpiod_chip_get_line(chip, HX711_SCK_PIN);

    if (!dt_line || !sck_line) {
        fprintf(stderr, "HX711: GPIO 라인 가져오기 실패\n");
        gpiod_chip_close(chip);
        chip = NULL;
        return false;
    }

    // DT를 입력으로, SCK를 출력으로 설정
    if (gpiod_line_request_input(dt_line, "hx711_dt") < 0) {
        fprintf(stderr, "HX711: DT 입력 설정 실패\n");
        gpiod_chip_close(chip);
        chip = NULL;
        return false;
    }

    if (gpiod_line_request_output(sck_line, "hx711_sck", 0) < 0) {
        fprintf(stderr, "HX711: SCK 출력 설정 실패\n");
        gpiod_chip_close(chip);
        chip = NULL;
        return false;
    }

    // SCK를 LOW로 초기화
    gpiod_line_set_value(sck_line, 0);
    delay_us(100);

    printf("HX711 초기화 완료 (DT=GPIO%d, SCK=GPIO%d)\n", HX711_DT_PIN, HX711_SCK_PIN);
    return true;
}

void hx711_close(void) {
    if (sck_line) {
        gpiod_line_release(sck_line);
        sck_line = NULL;
    }
    if (dt_line) {
        gpiod_line_release(dt_line);
        dt_line = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

bool hx711_is_ready(void) {
    if (!dt_line) return false;
    // DT가 LOW이면 데이터 준비됨
    return gpiod_line_get_value(dt_line) == 0;
}

int32_t hx711_read_raw(void) {
    if (!dt_line || !sck_line) return 0;

    // 데이터 준비될 때까지 대기 (timeout: 150ms)
    int timeout = 150000; // 150ms
    while (gpiod_line_get_value(dt_line) != 0) {
        delay_us(1);
        timeout--;
        if (timeout <= 0) {
            fprintf(stderr, "HX711: 데이터 준비 대기 타임아웃\n");
            return 0;
        }
    }

    int32_t value = 0;

    // 24비트 읽기 (MSB first)
    for (int i = 0; i < 24; i++) {
        gpiod_line_set_value(sck_line, 1);
        delay_short_us(1);
        gpiod_line_set_value(sck_line, 0);
        delay_short_us(1);

        int bit = gpiod_line_get_value(dt_line);
        value = (value << 1) | (bit & 1);
    }

    // 25번째 클럭 (gain = 128, channel A)
    gpiod_line_set_value(sck_line, 1);
    delay_short_us(1);
    gpiod_line_set_value(sck_line, 0);

    // 24비트 2의 보수 변환
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    return value;
}

float hx711_read_weight(void) {
    int32_t raw = hx711_read_raw();
    return (raw - hx711_offset) * hx711_scale;
}
