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
 * Fill the entire framebuffer with a solid color.
 */
void graphics_fill(uint16_t color);

/**
 * Draw a filled rectangle in the framebuffer.
 */
void graphics_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Draw a pixel in the framebuffer.
 */
void graphics_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * Copy a sprite bitmap into the framebuffer.
 * data must be an array of uint16_t pixels, row-major.
 */
void graphics_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data);

/**
 * Draw a single character using the built-in 6x8 font at (x, y).
 * Returns the x position after the character (for chaining).
 */
int graphics_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color);

/**
 * Draw a null-terminated string with the built-in font.
 * Wraps text within the given width. Returns the y position after drawing.
 */
int graphics_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width);

/**
 * Draw a horizontal rainbow (full hue sweep) bar.
 * Cycles through all hues left-to-right to test colour reproduction.
 */
void graphics_draw_rainbow_h(int16_t x, int16_t y, int16_t w, int16_t h);
