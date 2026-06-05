#include "app_nfc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>

#define PN532_I2C_BUS   "/dev/i2c-1"
#define PN532_I2C_ADDR  0x24

#define PN532_HOSTTOPN532       0xD4
#define CMD_SAMCONFIGURATION    0x14
#define CMD_INLISTPASSIVETARGET 0x4A

volatile NfcState g_nfc = {0};
static int i2c_fd = -1;
static pthread_t nfc_thread;
static volatile bool running = false;
static volatile int miss_count = 0;
#define MISS_THRESHOLD 2

static bool pn532_wait_ready(int timeout_ms) {
    uint8_t status = 0;
    for (int i = 0; i < timeout_ms / 10; i++) {
        if (read(i2c_fd, &status, 1) == 1 && (status & 0x01)) return true;
        usleep(10000);
    }
    return false;
}

static bool pn532_send(uint8_t *data, int len) {
    uint8_t frame[len + 8];
    int fi = 0;
    frame[fi++] = 0x00;
    frame[fi++] = 0x00;
    frame[fi++] = 0xFF;
    frame[fi++] = (uint8_t)len;
    frame[fi++] = (uint8_t)((~len + 1) & 0xFF);
    uint8_t dcs = 0;
    for (int i = 0; i < len; i++) { frame[fi++] = data[i]; dcs += data[i]; }
    frame[fi++] = (~dcs + 1) & 0xFF;
    frame[fi++] = 0x00;
    return write(i2c_fd, frame, fi) == fi;
}

static bool pn532_read_ack(void) {
    if (!pn532_wait_ready(200)) return false;
    uint8_t ack[7];
    return read(i2c_fd, ack, sizeof(ack)) == 7;
}

static int pn532_recv(uint8_t *buf, int max_len) {
    if (!pn532_wait_ready(500)) return -1;
    uint8_t tmp[64] = {0};
    if (read(i2c_fd, tmp, sizeof(tmp)) < 8) return -1;
    int data_len = tmp[4] - 2;
    if (data_len < 0 || data_len > max_len) return -1;
    memcpy(buf, tmp + 8, data_len);
    return data_len;
}

static bool pn532_sam_config(void) {
    uint8_t cmd[] = {PN532_HOSTTOPN532, CMD_SAMCONFIGURATION, 0x01, 0x14, 0x01};
    if (!pn532_send(cmd, sizeof(cmd))) return false;
    if (!pn532_read_ack()) return false;
    uint8_t resp[8];
    return pn532_recv(resp, sizeof(resp)) >= 0;
}

static int pn532_read_target(uint8_t *uid_out) {
    uint8_t cmd[] = {PN532_HOSTTOPN532, CMD_INLISTPASSIVETARGET, 0x01, 0x00};
    if (!pn532_send(cmd, sizeof(cmd))) return -1;
    if (!pn532_read_ack()) return -1;
    uint8_t resp[32];
    int len = pn532_recv(resp, sizeof(resp));
    if (len < 1 || resp[0] == 0) return 0;
    if (len < 7) return 0;
    uint8_t id_len = resp[5];
    if (id_len > 7 || len < (int)(6 + id_len)) return 0;
    memcpy(uid_out, resp + 6, id_len);
    return id_len;
}

static bool pn532_try_connect(void) {
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
    i2c_fd = open(PN532_I2C_BUS, O_RDWR);
    if (i2c_fd < 0) return false;
    if (ioctl(i2c_fd, I2C_SLAVE, PN532_I2C_ADDR) < 0) {
        close(i2c_fd);
        i2c_fd = -1;
        return false;
    }
    usleep(100000);
    if (!pn532_sam_config()) return false;
    g_nfc.connected = true;
    return true;
}

static void *nfc_thread_fn(void *arg) {
    (void)arg;
    while (running) {
        if (!g_nfc.connected) {
            pn532_try_connect();
            if (!g_nfc.connected) {
                usleep(3000000);
                continue;
            }
        }

        uint8_t uid[7];
        int uid_len = pn532_read_target(uid);
        if (uid_len > 0) {
            char buf[32] = {0};
            for (int i = 0; i < uid_len; i++) {
                char tmp[6];
                snprintf(tmp, sizeof(tmp), i == 0 ? "%02X" : " %02X", uid[i]);
                strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
            }
            g_nfc.tag_detected = true;
            g_nfc.uid_len = uid_len;
            strncpy((char *)g_nfc.uid_str, buf, sizeof(g_nfc.uid_str) - 1);
            miss_count = 0;
        } else if (uid_len < 0) {
            g_nfc.connected = false;
        } else {
            miss_count++;
            if (miss_count >= MISS_THRESHOLD) {
                g_nfc.tag_detected = false;
                miss_count = MISS_THRESHOLD;
            }
        }
        usleep(200000);
    }
    return NULL;
}

bool app_nfc_init(void) {
    if (pn532_try_connect()) return true;
    g_nfc.connected = false;
    return true;
}

void app_nfc_start(void) {
    running = true;
    pthread_create(&nfc_thread, NULL, nfc_thread_fn, NULL);
}

void app_nfc_stop(void) {
    running = false;
    pthread_join(nfc_thread, NULL);
    if (i2c_fd >= 0) close(i2c_fd);
}
