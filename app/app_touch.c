#include "app_touch.h"
#include <gpiod.h>
#include <unistd.h>
#include <stdio.h>

// BCM 번호
#define PIN_CLK  17
#define PIN_DIN  27
#define PIN_DO   22
#define PIN_CS   23
#define PIN_IRQ  18

// 캘리브레이션 (화면 안맞으면 조정)
#define X_MIN  506
#define X_MAX  3217
#define Y_MIN  618
#define Y_MAX  3592
#define SCREEN_W 240
#define SCREEN_H 320

static struct gpiod_chip *chip = NULL;
static struct gpiod_line *clk_line = NULL;
static struct gpiod_line *din_line = NULL;
static struct gpiod_line *do_line  = NULL;
static struct gpiod_line *cs_line  = NULL;
static struct gpiod_line *irq_line = NULL;

// ── 소프트웨어 SPI 전송 ─────────────────────────
static uint16_t xpt2046_transfer(uint8_t cmd) {
    uint16_t result = 0;

    // 8비트 명령 전송
    for (int i = 7; i >= 0; i--) {
        gpiod_line_set_value(din_line, (cmd >> i) & 1);
        gpiod_line_set_value(clk_line, 1);
        usleep(1);
        gpiod_line_set_value(clk_line, 0);
        usleep(1);
    }

    // 16비트 응답 수신
    for (int i = 15; i >= 0; i--) {
        gpiod_line_set_value(clk_line, 1);
        usleep(1);
        result |= ((uint16_t)gpiod_line_get_value(do_line) << i);
        gpiod_line_set_value(clk_line, 0);
        usleep(1);
    }

    return result >> 3; // 12비트 결과
}

bool xpt2046_read_raw(int16_t *rx, int16_t *ry) {
    // IRQ 핀 확인 (LOW면 터치됨)
    if (gpiod_line_get_value(irq_line) != 0) return false;

    gpiod_line_set_value(cs_line, 0);
    usleep(10);

    // 여러번 읽어서 평균 (노이즈 제거)
    int32_t x_sum = 0, y_sum = 0;
    int samples = 4;
    for (int i = 0; i < samples; i++) {
        x_sum += xpt2046_transfer(0xD0); // X 채널
        y_sum += xpt2046_transfer(0x90); // Y 채널
    }

    gpiod_line_set_value(cs_line, 1);

    *rx = (int16_t)(x_sum / samples);
    *ry = (int16_t)(y_sum / samples);
    return true;
}

// ── 공개 함수 ────────────────────────────────────
void app_touch_init(void) {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) { perror("touch: GPIO chip open 실패"); return; }

    clk_line = gpiod_chip_get_line(chip, PIN_CLK);
    din_line = gpiod_chip_get_line(chip, PIN_DIN);
    do_line  = gpiod_chip_get_line(chip, PIN_DO);
    cs_line  = gpiod_chip_get_line(chip, PIN_CS);
    irq_line = gpiod_chip_get_line(chip, PIN_IRQ);

    gpiod_line_request_output(clk_line, "touch", 0);
    gpiod_line_request_output(din_line, "touch", 0);
    gpiod_line_request_input(do_line,   "touch");
    gpiod_line_request_output(cs_line,  "touch", 1);
    gpiod_line_request_input(irq_line,  "touch");

}

void app_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    static int16_t last_x = 0, last_y = 0;

    int16_t raw_x, raw_y;
    if (xpt2046_read_raw(&raw_x, &raw_y)) {
        // 좌표 변환 (범위 클램프 + 스케일링)
        if (raw_x < X_MIN) raw_x = X_MIN;
        if (raw_x > X_MAX) raw_x = X_MAX;
        if (raw_y < Y_MIN) raw_y = Y_MIN;
        if (raw_y > Y_MAX) raw_y = Y_MAX;

        last_x = (int16_t)((raw_x - X_MIN) * SCREEN_W / (X_MAX - X_MIN));
        last_y = (int16_t)(SCREEN_H - 1 - (raw_y - Y_MIN) * SCREEN_H / (Y_MAX - Y_MIN));

        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}