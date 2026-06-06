#pragma once

#include <stdint.h>
#include <stdbool.h>

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// Pin configuration (ESP32-C6 SPI2 IOMUX)
#define TFT_CS   2
#define TFT_DC   1
#define TFT_RST  0
#define TFT_BL   3
#define TFT_MOSI 7
#define TFT_SCLK 6

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
 * Initialize the ST7789 display and allocate framebuffer.
 * Must be called once before any other display functions.
 */
void display_init(void);

/**
 * Fill the entire framebuffer with a solid color.
 */
void display_fill(uint16_t color);

/**
 * Draw a filled rectangle in the framebuffer.
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Draw a pixel in the framebuffer.
 */
void display_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * Copy a sprite bitmap into the framebuffer.
 * data must be an array of uint16_t RGB565 pixels, row-major.
 */
void display_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data);

/**
 * Draw a single character using the built-in 6x8 font at (x, y).
 * Returns the x position after the character (for chaining).
 */
int display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color);

/**
 * Draw a null-terminated string with the built-in font.
 * Wraps text within the given width. Returns the y position after drawing.
 */
int display_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width);

/**
 * Flush the framebuffer to the physical display via SPI.
 * Only the dirty region is sent (partial update).
 */
void display_flush(void);

/**
 * Set the backlight brightness (0-100).
 */
void display_set_backlight(uint8_t percent);

/**
 * Get a pointer to the framebuffer for direct manipulation.
 * Size is DISPLAY_WIDTH * DISPLAY_HEIGHT pixels (RGB565 = 2 bytes each).
 */
uint16_t *display_get_framebuffer(void);
