#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ── Standard gamepad state ────────────────────────────────────────── */

typedef struct {
    int32_t  axis_x;      /* Left stick X */
    int32_t  axis_y;      /* Left stick Y */
    int32_t  axis_rx;     /* Right stick X */
    int32_t  axis_ry;     /* Right stick Y */
    int32_t  brake;       /* LT */
    int32_t  throttle;    /* RT */
    uint16_t buttons;     /* Button bitmask */
} gamepad_state_t;

/* ── Generic button mapping ────────────────────────────────────────── */

typedef enum {
    CTRL_A,
    CTRL_B,
    CTRL_X,
    CTRL_Y,
    CTRL_LB,
    CTRL_RB,
    CTRL_DPAD_UP,
    CTRL_DPAD_DOWN,
    CTRL_DPAD_LEFT,
    CTRL_DPAD_RIGHT,
    CTRL_START,
    CTRL_SELECT,
    CTRL_COUNT
} ctrl_button_t;

void controller_init(void);
void controller_poll(void);
bool controller_is_connected(void);
bool controller_button_pressed(ctrl_button_t btn);
bool controller_button_is_pressed(ctrl_button_t btn);
void controller_set_button(gamepad_state_t *gp, ctrl_button_t btn, bool pressed);
gamepad_state_t controller_get_state(void);
