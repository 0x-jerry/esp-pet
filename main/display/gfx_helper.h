/**
 * @file gfx_helper.h
 * @brief Pixel-buffer helpers and common gfx_img factory functions.
 *
 * Colour constants, buffer fill utilities, and convenience wrappers
 * for creating gfx_img widgets from solid-colour or gradient buffers.
 */

#pragma once

#include <stdint.h>
#include "gfx.h"

/* ── Common RGB565 colour constants ───────────────────────────── */

extern const gfx_color_t GFX_COLOR_WHITE;
extern const gfx_color_t GFX_COLOR_RED;
extern const gfx_color_t GFX_COLOR_GREEN;
extern const gfx_color_t GFX_COLOR_BLUE;
extern const gfx_color_t GFX_COLOR_YELLOW;
extern const gfx_color_t GFX_COLOR_CYAN;
extern const gfx_color_t GFX_COLOR_GRAY;
extern const gfx_color_t GFX_COLOR_DARK_GRAY;

/* ── Buffer helpers ───────────────────────────────────────────── */

/** Fill a uint16_t pixel buffer with a solid colour. */
void gfx_buf_fill(uint16_t *buf, int w, int h, uint16_t color);

/** Fill a uint16_t pixel buffer with a horizontal rainbow gradient. */
void gfx_buf_rainbow_h(uint16_t *buf, int w, int h);

/** Initialize a gfx_image_dsc_t for a pre-filled pixel buffer.
 *  Zeroes the descriptor then sets header magic / cf / w / h / stride /
 *  data_size / data.  The caller owns filling the buffer. */
void gfx_init_dsc(gfx_image_dsc_t *dsc, uint16_t *buf, int w, int h);

/** Populate a gfx_image_dsc_t for a solid-colour buffer.
 *  Calls gfx_init_dsc() then fills buf with @p color. */
void gfx_make_solid_dsc(gfx_image_dsc_t *dsc, uint16_t *buf,
                        int w, int h, uint16_t color);

/* ── Widget factories ─────────────────────────────────────────── */

/** Create a gfx_img widget at (x, y) filled with a solid colour.
 *  @param out_buf  If non-NULL, stores the allocated pixel buffer pointer. */
gfx_obj_t *gfx_make_rect_img(gfx_disp_t *disp, int x, int y,
                             int w, int h, uint16_t color,
                             uint16_t **out_buf);
