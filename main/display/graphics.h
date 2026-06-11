#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "display.h"

// Helper: convert R/G/B (0-255) to packed 16-bit colour (RGB 5-6-5)
#define COLOR(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define COLOR_BLACK     COLOR(0, 0, 0)
#define COLOR_WHITE     COLOR(255, 255, 255)
#define COLOR_RED       COLOR(255, 0, 0)
#define COLOR_GREEN     COLOR(0, 255, 0)
#define COLOR_BLUE      COLOR(0, 0, 255)
#define COLOR_YELLOW    COLOR(255, 255, 0)
#define COLOR_CYAN      COLOR(0, 255, 255)
#define COLOR_MAGENTA   COLOR(255, 0, 255)
#define COLOR_GRAY      COLOR(128, 128, 128)
#define COLOR_DARK_GRAY COLOR(64, 64, 64)

/**
 * Begin a strip rendering context. All subsequent drawing calls operate
 * on the given strip buffer and clip to rows [y0, y0+strip_h).
 *
 * @param buf      Pointer to the DMA-safe strip buffer.
 * @param y0       Top row of this strip in screen coordinates.
 * @param strip_h  Height of this strip in rows.
 */
void graphics_begin_strip(uint16_t *buf, int16_t y0, int16_t strip_h);

/**
 * End the current strip rendering context.
 */
void graphics_end_strip(void);

/**
 * Draw a filled rectangle in the current strip.
 * Automatically clips to strip boundaries; out-of-strip rows are no-ops.
 */
void graphics_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Draw a pixel in the current strip buffer.
 * Silently clips out-of-bounds or out-of-strip coordinates.
 */
void graphics_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * Copy a sprite bitmap into the current strip.
 * data must be an array of uint16_t pixels, row-major.
 * Only rows overlapping the current strip are drawn.
 */
void graphics_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data);

/**
 * Draw a sprite using a region map + palette.
 *
 * The region map is a uint8_t array of size w×h, where each value is an index
 * into the 16-entry palette. Index 0 is transparent (skipped).
 * Only rows overlapping the current strip are drawn.
 *
 * @param x, y     Top-left screen position.
 * @param w, h     Dimensions of the region map.
 * @param map      uint8_t[w*h] region ID per pixel.
 * @param palette  uint16_t[16] color lookup table. palette[0] is transparent.
 */
void graphics_draw_sprite_region(int16_t x, int16_t y, int16_t w, int16_t h,
                                 const uint8_t *map, const uint16_t *palette);

/**
 * Draw a null-terminated string with the built-in font.
 * Wraps text within the given width. Returns the y position after drawing.
 */
int graphics_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width);

/**
 * Draw a null-terminated string with transparent background.
 * Only foreground pixels are written; existing pixels show through.
 * Wraps text within the given width. Returns the y position after drawing.
 */
int graphics_draw_text_fg(int16_t x, int16_t y, const char *text, uint16_t color, int16_t max_width);

/**
 * Draw a horizontal rainbow (full hue sweep) bar.
 * Cycles through all hues left-to-right to test colour reproduction.
 */
void graphics_draw_rainbow_h(int16_t x, int16_t y, int16_t w, int16_t h);

/**
 * Draw a filled rounded rectangle with a 1-pixel border.
 * Uses a circle approximation for the corners (radius = 6).
 */
void graphics_draw_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t bg, uint16_t border);
