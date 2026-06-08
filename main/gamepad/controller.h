#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Standard gamepad state structure.
 *
 * Encapsulates the full state of a gamepad at a given moment,
 * including both analog axes and digital button bitmask.
 */
typedef struct {
    int16_t  axis_x;      /**< Left stick X axis (-32768 to 32767, center 0) */
    int16_t  axis_y;      /**< Left stick Y axis (-32768 to 32767, center 0) */
    int16_t  axis_rx;     /**< Right stick X axis (-32768 to 32767, center 0) */
    int16_t  axis_ry;     /**< Right stick Y axis (-32768 to 32767, center 0) */
    int16_t  brake;       /**< Left trigger / brake (0 to 1023) */
    int16_t  throttle;    /**< Right trigger / throttle (0 to 1023) */
    uint16_t buttons;     /**< Button bitmask, one bit per ctrl_button_t */
} gamepad_state_t;

/**
 * @brief Generic button mapping enum.
 *
 * Provides a controller-agnostic set of button identifiers.
 * Each supported controller protocol maps its native buttons
 * to these standard names.
 */
typedef enum {
    CTRL_A,               /**< Face button A (bottom) */
    CTRL_B,               /**< Face button B (right) */
    CTRL_X,               /**< Face button X (left) */
    CTRL_Y,               /**< Face button Y (top) */
    CTRL_LB,              /**< Left bumper / shoulder */
    CTRL_RB,              /**< Right bumper / shoulder */
    CTRL_DPAD_UP,         /**< D-pad up */
    CTRL_DPAD_DOWN,       /**< D-pad down */
    CTRL_DPAD_LEFT,       /**< D-pad left */
    CTRL_DPAD_RIGHT,      /**< D-pad right */
    CTRL_START,           /**< Start / Menu button */
    CTRL_SELECT,          /**< Select / View button */
} ctrl_button_t;

/** @brief Initialize the controller subsystem. */
void controller_init(void);

/** @brief Poll the controller for the latest state update. */
void controller_poll(void);

/**
 * @brief Check whether a controller is currently connected.
 * @return true if a controller is connected, false otherwise.
 */
bool controller_is_connected(void);

/**
 * @brief Check if a button was just pressed (rising edge).
 *
 * Returns true only once per press — when the button transitions
 * from released to pressed.
 *
 * @param btn The button to check.
 * @return true if the button was just pressed this frame.
 */
bool controller_button_pressed(ctrl_button_t btn);

/**
 * @brief Check if a button is currently held down (level).
 *
 * Returns true every frame while the button remains pressed.
 *
 * @param btn The button to check.
 * @return true if the button is currently held.
 */
bool controller_button_is_pressed(ctrl_button_t btn);

/**
 * @brief Set the pressed state of a button in a gamepad state.
 *
 * Helper used by controller backends to update the button bitmask.
 *
 * @param gp     Pointer to the gamepad state to modify.
 * @param btn    The button to set.
 * @param pressed true to mark as pressed, false to mark as released.
 */
void controller_set_button(gamepad_state_t *gp, ctrl_button_t btn, bool pressed);

/**
 * @brief Get a copy of the current gamepad state.
 * @return The most recently polled gamepad_state_t.
 */
gamepad_state_t controller_get_state(void);
