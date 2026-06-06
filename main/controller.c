#include "controller.h"
#include "xbox/xbox_hid.h"
#include "xbox/xbox_ble.h"
#include <string.h>

static uint16_t g_prev_buttons = 0;
static uint16_t g_curr_buttons = 0;

static uint16_t btn_bit(ctrl_button_t btn) {
    return (uint16_t)(1 << (int)btn);
}

static uint16_t build_mask(const xbox_gamepad_t *gp) {
    uint16_t mask = 0;
    if (gp->buttons & XB_BTN_A)           mask |= btn_bit(CTRL_A);
    if (gp->buttons & XB_BTN_B)           mask |= btn_bit(CTRL_B);
    if (gp->buttons & XB_BTN_X)           mask |= btn_bit(CTRL_X);
    if (gp->buttons & XB_BTN_Y)           mask |= btn_bit(CTRL_Y);
    if (gp->buttons & XB_BTN_LB)          mask |= btn_bit(CTRL_LB);
    if (gp->buttons & XB_BTN_RB)          mask |= btn_bit(CTRL_RB);
    if (gp->buttons & XB_DPAD_UP)         mask |= btn_bit(CTRL_DPAD_UP);
    if (gp->buttons & XB_DPAD_DOWN)       mask |= btn_bit(CTRL_DPAD_DOWN);
    if (gp->buttons & XB_DPAD_LEFT)       mask |= btn_bit(CTRL_DPAD_LEFT);
    if (gp->buttons & XB_DPAD_RIGHT)      mask |= btn_bit(CTRL_DPAD_RIGHT);
    if (gp->buttons & XB_BTN_START)       mask |= btn_bit(CTRL_START);
    if (gp->buttons & XB_BTN_SELECT)      mask |= btn_bit(CTRL_SELECT);
    return mask;
}

void controller_init(void) {
    g_prev_buttons = 0;
    g_curr_buttons = 0;
}

void controller_poll(void) {
    xbox_gamepad_t gp = xbox_ble_get_gamepad();
    g_prev_buttons = g_curr_buttons;
    g_curr_buttons = build_mask(&gp);
}

bool controller_is_connected(void) {
    return xbox_ble_is_connected();
}

bool controller_button_pressed(ctrl_button_t btn) {
    uint16_t bit = btn_bit(btn);
    return (g_curr_buttons & bit) && !(g_prev_buttons & bit);
}
