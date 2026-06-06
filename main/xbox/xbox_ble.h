#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "xbox_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the NimBLE Xbox BLE stack and start scanning.
 * Must be called from the NimBLE host task context (on_sync callback).
 */
void xbox_ble_init(int max_devices);

/**
 * Thread-safe read of the latest parsed gamepad state.
 */
xbox_gamepad_t xbox_ble_get_gamepad(void);

/**
 * Returns true when the controller is connected AND subscribed.
 */
bool xbox_ble_is_connected(void);

#ifdef __cplusplus
}
#endif
