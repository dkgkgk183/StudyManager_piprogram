#include "ili9341.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SPI_DEVICE   "/dev/spidev0.0"
#define SPI_SPEED_HZ  16000000   // 16MHz (안정적)

#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_PASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_PIXFMT  0x3A

static int spi_fd = -1;
static struct gpiod_chip *gpio_chip = NULL;
static struct gpiod_line *dc_line   = NULL;
static struct gpiod_line *reset_line = NULL;

static void spi_write(const uint8_t *data, size_t len) {
    const size_t CHUNK = 4096;
    size_t offset = 0;
    while (offset < len) {
        size_t chunk_len = (len - offset) > CHUNK ? CHUNK : (len - offset);
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)(data + offset),
            .len    = chunk_len,
            .speed_hz = SPI_SPEED_HZ,
            .bits_per_word = 8,
        };
        if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0)
            perror("SPI write failed");
        offset += chunk_len;
    }
}

static void send_cmd(uint8_t cmd) {
    gpiod_line_set_value(dc_line, 0);
    spi_write(&cmd, 1);
}

static void send_data(const uint8_t *data, size_t len) {
    gpiod_line_set_value(dc_line, 1);
    spi_write(data, len);
}

static void send_byte(uint8_t b) { send_data(&b, 1); }

void ili9341_init(void) {
    // SPI 초기화
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        perror("SPI open 실패 - raspi-config에서 SPI 활성화 확인");
        exit(1);
    }
    uint8_t mode = SPI_MODE_0, bits = 8;
    uint32_t speed = SPI_SPEED_HZ;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // GPIO 초기화
    gpio_chip = gpiod_chip_open("/dev/gpiochip0");
    if (!gpio_chip) { perror("GPIO chip open 실패"); exit(1); }

    dc_line    = gpiod_chip_get_line(gpio_chip, GPIO_DC);
    reset_line = gpiod_chip_get_line(gpio_chip, GPIO_RESET);
    gpiod_line_request_output(dc_line,    "ili9341", 1);
    gpiod_line_request_output(reset_line, "ili9341", 1);

    // 하드웨어 리셋
    gpiod_line_set_value(reset_line, 1); usleep(10000);
    gpiod_line_set_value(reset_line, 0); usleep(10000);
    gpiod_line_set_value(reset_line, 1); usleep(150000);

    // 초기화 시퀀스
    send_cmd(CMD_SWRESET); usleep(150000);
    send_cmd(CMD_SLPOUT);  usleep(500000);

    send_cmd(CMD_PIXFMT); send_byte(0x55);  // 16비트 RGB565
    send_cmd(CMD_MADCTL);  send_byte(0x48); // Portrait, BGR

    send_cmd(CMD_DISPON);
    usleep(100000);

    printf("ILI9341 초기화 완료\n");
}

static void set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    send_cmd(CMD_CASET);
    uint8_t c[4] = {x1 >> 8, x1 & 0xFF, x2 >> 8, x2 & 0xFF};
    send_data(c, 4);

    send_cmd(CMD_PASET);
    uint8_t r[4] = {y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF};
    send_data(r, 4);

    send_cmd(CMD_RAMWR);
}

void ili9341_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    // v9에서 LV_COLOR_16_SWAP 대신 직접 스왑
    lv_draw_sw_rgb565_swap(px_map, w * h);

    set_window(area->x1, area->y1, area->x2, area->y2);
    send_data(px_map, w * h * 2);
    lv_display_flush_ready(disp);
}