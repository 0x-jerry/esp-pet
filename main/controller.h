#pragma once

#include <stdbool.h>
#include <stdint.h>

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
bool controller_is_connected(void);
bool controller_button_pressed(ctrl_button_t btn);
