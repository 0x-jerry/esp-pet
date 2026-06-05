#include "controller.h"
#include "my_platform.h"
#include <uni.h>

static uint16_t g_prev_buttons = 0;

void controller_init(void) {
}

bool controller_is_connected(void) {
    return my_platform_is_connected();
}

static uint16_t btn_bit(ctrl_button_t btn) {
    switch (btn) {
    case CTRL_A:          return 1 << 0;
    case CTRL_B:          return 1 << 1;
    case CTRL_X:          return 1 << 2;
    case CTRL_Y:          return 1 << 3;
    case CTRL_LB:         return 1 << 4;
    case CTRL_RB:         return 1 << 5;
    case CTRL_DPAD_UP:    return 1 << 6;
    case CTRL_DPAD_DOWN:  return 1 << 7;
    case CTRL_DPAD_LEFT:  return 1 << 8;
    case CTRL_DPAD_RIGHT: return 1 << 9;
    case CTRL_START:      return 1 << 10;
    case CTRL_SELECT:     return 1 << 11;
    default:              return 0;
    }
}

static uint16_t build_mask(const uni_gamepad_t *gp) {
    uint16_t mask = 0;
    if (gp->buttons & BUTTON_A)          mask |= btn_bit(CTRL_A);
    if (gp->buttons & BUTTON_B)          mask |= btn_bit(CTRL_B);
    if (gp->buttons & BUTTON_X)          mask |= btn_bit(CTRL_X);
    if (gp->buttons & BUTTON_Y)          mask |= btn_bit(CTRL_Y);
    if (gp->buttons & BUTTON_SHOULDER_L) mask |= btn_bit(CTRL_LB);
    if (gp->buttons & BUTTON_SHOULDER_R) mask |= btn_bit(CTRL_RB);
    if (gp->buttons & MISC_BUTTON_START) mask |= btn_bit(CTRL_START);
    if (gp->buttons & MISC_BUTTON_SELECT) mask |= btn_bit(CTRL_SELECT);
    if (gp->dpad & DPAD_UP)              mask |= btn_bit(CTRL_DPAD_UP);
    if (gp->dpad & DPAD_DOWN)            mask |= btn_bit(CTRL_DPAD_DOWN);
    if (gp->dpad & DPAD_LEFT)            mask |= btn_bit(CTRL_DPAD_LEFT);
    if (gp->dpad & DPAD_RIGHT)           mask |= btn_bit(CTRL_DPAD_RIGHT);
    return mask;
}

bool controller_button_pressed(ctrl_button_t btn) {
    uni_gamepad_t gp = my_platform_get_gamepad();
    uint16_t current = build_mask(&gp);
    uint16_t bit = btn_bit(btn);
    bool pressed = ((current & bit) && !(g_prev_buttons & bit));
    g_prev_buttons = current;
    return pressed;
}
