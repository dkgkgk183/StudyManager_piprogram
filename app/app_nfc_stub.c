/*
 * app_nfc_stub.c - PC 데모용 NFC 스텁
 *
 * PN532 I2C 하드웨어 없이도 NFC API를 안전하게 호출할 수 있도록
 * 항상 "연결 안 됨 / 태그 없음" 상태를 유지한다.
 */

#include "app_nfc.h"
#include <string.h>

volatile NfcState g_nfc = {0};

bool app_nfc_init(void) {
    memset((void *)&g_nfc, 0, sizeof(g_nfc));
    g_nfc.connected    = false;
    g_nfc.tag_detected = false;
    return true;
}

void app_nfc_start(void) {
    /* PC에서는 폴링 스레드 없이도 무방. 상태만 노출. */
}

void app_nfc_stop(void) {
    /* no-op */
}
