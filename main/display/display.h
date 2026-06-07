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
#define TFT_BL   4
#define TFT_MOSI 7
#define TFT_SCLK 6

/**
 * Initialize the ST7789 display and allocate framebuffer.
 * Must be called once before any other display functions.
 */
void display_init(void);

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
 * Size is DISPLAY_WIDTH * DISPLAY_HEIGHT pixels (2 bytes each).
 */
uint16_t *display_get_framebuffer(void);

/**
 * Mark the framebuffer as dirty, ensuring the next flush() sends the update.
 * Called automatically by the graphics_* drawing functions.
 */
void display_mark_dirty(void);
