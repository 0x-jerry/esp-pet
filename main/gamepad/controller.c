#include "controller.h"
#include "xbox_hid.h"
#include "xbox_ble.h"
#include <string.h>

static gamepad_state_t g_state = {0};
static uint16_t g_prev_buttons = 0;

static uint16_t btn_bit(ctrl_button_t btn) {
    return (uint16_t)(1 << (int)btn);
}

void controller_init(void) {
}

void controller_poll(void) {
    g_prev_buttons = g_state.buttons;
    g_state = xbox_ble_get_gamepad();
}

bool controller_is_connected(void) {
    return xbox_ble_is_connected();
}

bool controller_button_pressed(ctrl_button_t btn) {
    uint16_t bit = btn_bit(btn);
    return g_state.buttons & bit && !(g_prev_buttons & bit);
}

bool controller_button_is_pressed(ctrl_button_t btn) {
    uint16_t bit = btn_bit(btn);
    return g_state.buttons & bit;
}

void controller_set_button(gamepad_state_t *gp, ctrl_button_t btn, bool pressed)
{
    uint16_t bit = btn_bit(btn);

    if (pressed) {
        gp->buttons |= bit;
    } else {
        gp->buttons &= ~bit;
    }
}

gamepad_state_t controller_get_state(void) {
    return g_state;
}
