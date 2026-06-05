#pragma once

#include <stdint.h>

#define FONT_WIDTH  6
#define FONT_HEIGHT 8

/**
 * Get the bitmap for a character (6x8 pixels, 6 bytes per char).
 * Each byte is a column, LSB at top.
 * Returns NULL if character is not in the font.
 */
const uint8_t *font_get_char(char c);
