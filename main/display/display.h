#pragma once

#include <stdint.h>
#include <stdbool.h>

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// Strip height for strip-based rendering (1/10 of display → ~11 KB per strip)
#define DISPLAY_STRIP_H (DISPLAY_HEIGHT / 10)

// Pin configuration (ESP32-C6 SPI2 IOMUX)
#define TFT_CS   2
#define TFT_DC   1
#define TFT_RST  0
#define TFT_BL   4
#define TFT_MOSI 7
#define TFT_SCLK 6

/**
 * Initialize the ST7789 display and allocate the DMA-safe strip buffer.
 * Must be called once before any other display functions.
 */
void display_init(void);

/**
 * Set the backlight brightness (0-100).
 */
void display_set_backlight(uint8_t percent);

/**
 * Get a pointer to the current strip buffer (DMA-safe, DISPLAY_WIDTH * DISPLAY_STRIP_H pixels).
 * Valid only within a render callback passed to display_render_frame().
 */
uint16_t *display_get_strip_buf(void);

/**
 * Render one full frame using strip-based rendering.
 *
 * For each strip (0..9), the strip buffer is zeroed, then `render_cb(y0, y1)`
 * is called so the caller can draw all elements that overlap [y0, y1).
 * After the callback returns, the strip is DMA'd to the LCD.
 *
 * @param render_cb  Called once per strip with the strip's row range [y0, y1).
 */
void display_render_frame(void (*render_cb)(int16_t y0, int16_t y1));
