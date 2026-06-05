#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BTN_ACTION 14
#define BTN_TALK   15

/**
 * Initialize both buttons with internal pull-ups and edge-triggered interrupts.
 * A 200 ms software debounce is applied per button.
 */
void buttons_init(void);

/**
 * Check whether the Action button was pressed since the last call.
 * Returns true once per button press.
 */
bool button_action_pressed(void);

/**
 * Check whether the Talk button was pressed since the last call.
 * Returns true once per button press.
 */
bool button_talk_pressed(void);
