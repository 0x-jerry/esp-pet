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
static gamepad_state_t parse_hid(const uint8_t *data) {
    gamepad_state_t gp = {0};

    controller_set_button(&gp, CTRL_A, data[13] & (1 << 0));
    controller_set_button(&gp, CTRL_B, data[13] & (1 << 1));
    controller_set_button(&gp, CTRL_X, data[13] & (1 << 3));
    controller_set_button(&gp, CTRL_Y, data[13] & (1 << 4));
    controller_set_button(&gp, CTRL_LB, data[13] & (1 << 6));
    controller_set_button(&gp, CTRL_RB, data[13] & (1 << 7));

    controller_set_button(&gp, CTRL_DPAD_UP, data[12] == 0x01 || data[12] == 0x02 || data[12] == 0x08);
    controller_set_button(&gp, CTRL_DPAD_RIGHT, data[12] == 0x02 || data[12] == 0x03 || data[12] == 0x04);
    controller_set_button(&gp, CTRL_DPAD_DOWN, data[12] == 0x04 || data[12] == 0x05 || data[12] == 0x06);
    controller_set_button(&gp, CTRL_DPAD_LEFT, data[12] == 0x06 || data[12] == 0x07 || data[12] == 0x08);

    controller_set_button(&gp, CTRL_START, data[14] & (1 << 2));
    controller_set_button(&gp, CTRL_SELECT, data[14] & (1 << 3));

    gp.axis_x   = (int16_t)(data[1] - 0x80);
    gp.axis_y   = (int16_t)(data[3] - 0x80);
    gp.axis_rx  = (int16_t)(data[5] - 0x80);
    gp.axis_ry  = (int16_t)(data[7] - 0x80);

    gp.brake    = (int16_t)data[8];
    gp.throttle = (int16_t)data[10];

    return gp;
}

/* ── Public API ─────────────────────────────────────────────────────── */

gamepad_state_t xbox_hid_parse(const uint8_t *data, uint16_t len) {
    if (data == NULL) {
        return (gamepad_state_t){0};
    }

    ESP_LOGI(TAG, "HID report 0:%02X 1:%02X 2:%02X 3:%02X 4:%02X 5:%02X 6:%02X 7:%02X 8:%02X 9:%02X 10:%02X 11:%02X 12:%02X 13:%02X 14:%02X 15:%02X", 
        data[0], data[1], data[2], data[3],
        data[4], data[5], data[6], data[7],
        data[8], data[9], data[10], data[11],
        data[12], data[13], data[14], data[15]
    );

    return parse_hid(data);
}
