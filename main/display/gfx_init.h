/**
 * @file gfx_init.h
 * @brief esp_emote_gfx initialization wrapper.
 *
 * Initializes the gfx core, attaches the ST7789 display,
 * and provides a display handle for widget creation.
 */

#pragma once

/* Forward declarations — no need to pull in the full gfx.h here. */
typedef struct gfx_disp gfx_disp_t;
typedef void *gfx_handle_t;

/**
 * Initialize esp_emote_gfx with the ST7789 display.
 * Calls gfx_emote_init() and gfx_disp_add() internally.
 *
 * @return gfx_disp_t pointer for widget creation, or NULL on failure.
 */
gfx_disp_t *gfx_init(void);

/**
 * Deinitialize esp_emote_gfx.
 * @param handle  The handle returned by gfx_emote_init().
 */
void gfx_deinit(gfx_handle_t handle);
