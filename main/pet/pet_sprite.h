#pragma once

#include <stdint.h>
#include "pet_state.h"

/** Sprite dimensions (square). */
#define PET_SPRITE_SIZE 48

/** Animation frame count for idle cycle. */
#define PET_SPRITE_FRAMES 2

/** Frame interval for idle animation (milliseconds). */
#define PET_ANIM_INTERVAL_MS 500

/**
 * Draw the current pet sprite frame at (x, y) on the framebuffer.
 * Automatically advances animation based on elapsed time.
 *
 * @param x       Top-left X position.
 * @param y       Top-left Y position.
 * @param mood    Current pet mood (affects color variant).
 * @param sleeping If true, draws a sleep-blep variant instead.
 */
void pet_sprite_draw(int16_t x, int16_t y, pet_mood_t mood, bool sleeping);

/**
 * Reset the animation timer (e.g., on mood change to force a redraw).
 */
void pet_sprite_reset_animation(void);
