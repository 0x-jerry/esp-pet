#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "controller.h"

/* ── HID report parser ──────────────────────────────────────────────── */

/**
 * Parse a raw BLE HID input report into a gamepad_state_t.
 * Supports report ID 0x01 (17-byte) and 0x19 (16-byte).
 * Returns a zero-filled struct if the report is unrecognized.
 */
gamepad_state_t xbox_hid_parse(const uint8_t *data, uint16_t len);
