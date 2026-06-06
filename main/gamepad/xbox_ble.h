#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "controller.h"

/**
 * Initialize the NimBLE Xbox BLE stack and start scanning.
 * Must be called from the NimBLE host task context (on_sync callback).
 */
void xbox_ble_init(int max_devices);

/**
 * Thread-safe read of the latest parsed gamepad state.
 */
gamepad_state_t xbox_ble_get_gamepad(void);

/**
 * Returns true when the controller is connected AND subscribed.
 */
bool xbox_ble_is_connected(void);
