/**
 * xbox_hid.c — Xbox Wireless Controller HID report parser
 *
 * Pure data transformation: raw BLE notify bytes → xbox_gamepad_t.
 * No BLE, FreeRTOS, or hardware dependencies.
 */
#include "xbox_hid.h"
#include <string.h>

/* ── 0x01 report (legacy, 17-byte) ──────────────────────────────────── */
static xbox_gamepad_t parse_01(const uint8_t *data) {
    xbox_gamepad_t gp = {0};

    gp.axis_x   = (int32_t)(data[1]  | (data[2]  << 8));
    gp.axis_y   = (int32_t)(data[3]  | (data[4]  << 8));
    gp.axis_rx  = (int32_t)(data[5]  | (data[6]  << 8));
    gp.axis_ry  = (int32_t)(data[7]  | (data[8]  << 8));
    gp.brake    = (int32_t)((data[9]  | (data[10] << 8)) & 0x03FF);
    gp.throttle = (int32_t)((data[11] | (data[12] << 8)) & 0x03FF);
    gp.dpad     = data[13] & 0x0F;
    gp.buttons  = (uint16_t)(data[14] | (data[15] << 8));

    return gp;
}

/* ── 0x19 report (16-byte) ──────────────────────────────────────────── */
static xbox_gamepad_t parse_19(const uint8_t *data) {
    xbox_gamepad_t gp = {0};

    gp.axis_x   = (int32_t)(data[1]  | (data[2]  << 8));
    gp.axis_y   = (int32_t)(data[3]  | (data[4]  << 8));
    gp.axis_rx  = (int32_t)(data[5]  | (data[6]  << 8));
    gp.axis_ry  = (int32_t)(data[7]  | (data[8]  << 8));
    gp.brake    = (int32_t)((data[9]  | (data[10] << 8)) & 0x03FF);
    gp.throttle = (int32_t)((data[11] | (data[12] << 8)) & 0x03FF);
    gp.buttons  = (uint16_t)(data[13] | (data[14] << 8));
    gp.dpad     = 0;

    return gp;
}

/* ── Public API ─────────────────────────────────────────────────────── */

xbox_gamepad_t xbox_hid_parse(const uint8_t *data, uint16_t len) {
    if (data == NULL) {
        return (xbox_gamepad_t){0};
    }

    if (data[0] == 0x01 && len >= 17) {
        return parse_01(data);
    }

    if (data[0] == 0x19 && len >= 16) {
        return parse_19(data);
    }

    return (xbox_gamepad_t){0};
}
