#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Button bitmask definitions ─────────────────────────────────────── */

#define XB_BTN_A         (1 << 0)
#define XB_BTN_B         (1 << 1)
#define XB_BTN_X         (1 << 3)
#define XB_BTN_Y         (1 << 4)
#define XB_BTN_LB        (1 << 6)
#define XB_BTN_RB        (1 << 7)

#define XB_BTN_THUMBL    (1 << 2)
#define XB_BTN_THUMBR    (1 << 5)
#define XB_BTN_START     (1 << 8)
#define XB_BTN_SELECT    (1 << 9)
#define XB_BTN_XBOX      (1 << 10)
#define XB_BTN_SHARE     (1 << 11)
#define XB_DPAD_UP       (1 << 12)
#define XB_DPAD_DOWN     (1 << 13)
#define XB_DPAD_LEFT     (1 << 14)
#define XB_DPAD_RIGHT    (1 << 15)

/* ── Gamepad state ──────────────────────────────────────────────────── */

typedef struct {
    int32_t  axis_x;
    int32_t  axis_y;
    int32_t  axis_rx;
    int32_t  axis_ry;
    int32_t  brake;       /* LT */
    int32_t  throttle;    /* RT */
    uint16_t buttons;
    uint8_t  dpad;
} xbox_gamepad_t;

/* ── HID report parser ──────────────────────────────────────────────── */

/**
 * Parse a raw BLE HID input report into an xbox_gamepad_t.
 * Supports report ID 0x01 (17-byte) and 0x19 (16-byte).
 * Returns a zero-filled struct if the report is unrecognized.
 */
xbox_gamepad_t xbox_hid_parse(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
