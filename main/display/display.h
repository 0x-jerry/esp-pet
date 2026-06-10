#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_panel_ops.h"

/** Pack 8-bit R/G/B into a uint16_t RGB565 pixel. */
#define RGB565(r, g, b) \
    ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

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
 * Initialize the ST7789 display hardware (SPI, panel, backlight).
 * Must be called once before any other display functions.
 * Does NOT allocate a framebuffer — esp_emote_gfx manages that.
 */
void display_init(void);

/**
 * Get the ESP LCD panel handle for use with esp_emote_gfx flush callback.
 */
esp_lcd_panel_handle_t display_get_panel_handle(void);

/**
 * Set the backlight brightness (0-100).
 */
void display_set_backlight(uint8_t percent);
