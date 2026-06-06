#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Button bitmask definitions (matching Xbox v5 BLE report)
#define XB_BTN_A         (1 << 0)
#define XB_BTN_B         (1 << 1)
#define XB_BTN_X         (1 << 2)
#define XB_BTN_Y         (1 << 3)
#define XB_BTN_LB        (1 << 4)
#define XB_BTN_RB        (1 << 5)
#define XB_BTN_THUMBL    (1 << 6)
#define XB_BTN_THUMBR    (1 << 7)
#define XB_BTN_START     (1 << 8)
#define XB_BTN_SELECT    (1 << 9)
#define XB_BTN_XBOX      (1 << 10)
#define XB_BTN_SHARE     (1 << 11)
#define XB_DPAD_UP       (1 << 12)
#define XB_DPAD_DOWN     (1 << 13)
#define XB_DPAD_LEFT     (1 << 14)
#define XB_DPAD_RIGHT    (1 << 15)

typedef struct {
    int32_t  axis_x;
    int32_t  axis_y;
    int32_t  axis_rx;
    int32_t  axis_ry;
    int32_t  brake;
    int32_t  throttle;
    uint16_t buttons;
    uint8_t  dpad;
} xbox_gamepad_t;

void xbox_ble_init(int max_devices);
xbox_gamepad_t xbox_ble_get_gamepad(void);
bool xbox_ble_is_connected(void);

#ifdef __cplusplus
}
#endif
