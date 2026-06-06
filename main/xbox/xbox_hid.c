/**
 * xbox_hid.c — Xbox Wireless Controller HID report parser
 *
 * Pure data transformation: raw BLE notify bytes → xbox_gamepad_t.
 * No BLE, FreeRTOS, or hardware dependencies.
 */
#include "xbox_hid.h"
#include "esp_log.h"
#include <string.h>

#define TAG "xbox_hid"

/* ── HID report (16-byte) ──────────────────────────────────────────── */
static xbox_gamepad_t parse_hid(const uint8_t *data) {
    xbox_gamepad_t gp = {0};

    gp.buttons  = (uint16_t)(data[13] | (data[14] << 8));

    return gp;
}

/* ── Public API ─────────────────────────────────────────────────────── */

xbox_gamepad_t xbox_hid_parse(const uint8_t *data, uint16_t len) {
    if (data == NULL) {
        return (xbox_gamepad_t){0};
    }

    ESP_LOGI(TAG, "HID report 0:%02X 1:%02X 2:%02X 3:%02X 4:%02X 5:%02X 6:%02X 7:%02X 8:%02X 9:%02X 10:%02X 11:%02X 12:%02X 13:%02X 14:%02X 15:%02X", 
        data[0], data[1], data[2], data[3],
        data[4], data[5], data[6], data[7],
        data[8], data[9], data[10], data[11],
        data[12], data[13], data[14], data[15]
    );

    return parse_hid(data);
}
