#pragma once

#include <stdint.h>
#include "gfx.h"
#include "pet_state.h"

/** Sprite dimensions (square). */
#define PET_SPRITE_SIZE 48

/** Animation frame count for idle cycle. */
#define PET_SPRITE_FRAMES 2

/** Frame interval for idle animation (milliseconds). */
#define PET_ANIM_INTERVAL_MS 500

/**
 * Initialize pet sprite widgets on a gfx display.
 * Allocates pixel buffers and creates gfx_img objects.
 * Must be called once after gfx_disp_add().
 */
void pet_sprite_init(gfx_disp_t *disp);

/**
 * Update the pet sprite image (mood colour, animation frame).
 * Called every game tick.
 *
 * @param mood     Current pet mood (affects colour variant).
 * @param sleeping If true, draws the sleeping variant.
 */
void pet_sprite_update(pet_mood_t mood, bool sleeping);
