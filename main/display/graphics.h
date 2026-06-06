#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "display.h"

// Colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_DARK_GRAY 0x4208

// Color from RGB888 components
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

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
 * data must be an array of uint16_t RGB565 pixels, row-major.
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
