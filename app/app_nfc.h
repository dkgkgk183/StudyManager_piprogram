#ifndef APP_NFC_H
#define APP_NFC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool    connected;
    bool    tag_detected;
    char    uid_str[32];
    uint8_t uid_len;
} NfcState;

extern volatile NfcState g_nfc;

bool app_nfc_init(void);
void app_nfc_start(void);
void app_nfc_stop(void);

#endif