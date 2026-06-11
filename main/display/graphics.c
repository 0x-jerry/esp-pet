#include "graphics.h"
#include "font.h"
#include <string.h>

// ── Strip context (set by graphics_begin_strip) ─────────────

static uint16_t *g_strip_buf = NULL;
static int16_t   g_strip_y0  = 0;
static int16_t   g_strip_h   = 0;

void graphics_begin_strip(uint16_t *buf, int16_t y0, int16_t strip_h) {
    g_strip_buf = buf;
    g_strip_y0  = y0;
    g_strip_h   = strip_h;
}

void graphics_end_strip(void) {
    g_strip_buf = NULL;
    g_strip_y0  = 0;
    g_strip_h   = 0;
}

// ── Internal: pixel pointer in strip (or NULL if out of strip) ─

static inline uint16_t *fb_pixel(int16_t x, int16_t y) {
    if (x < 0 || x >= DISPLAY_WIDTH) return NULL;
    int16_t strip_y = y - g_strip_y0;
    if (strip_y < 0 || strip_y >= g_strip_h) return NULL;
    return &g_strip_buf[strip_y * DISPLAY_WIDTH + x];
}

// ── Pixel ───────────────────────────────────────────────────

void graphics_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    uint16_t *p = fb_pixel(x, y);
    if (p) *p = color;
}

// ── Filled rectangle ────────────────────────────────────────

void graphics_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Clip to screen
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    // Clip to strip
    int16_t strip_end = g_strip_y0 + g_strip_h;
    int16_t row_start = y;
    if (row_start < g_strip_y0) row_start = g_strip_y0;
    int16_t row_end = y + h;
    if (row_end > strip_end) row_end = strip_end;
    if (row_start >= row_end) return;

    // Fill each visible row
    int16_t strip_row0 = row_start - g_strip_y0;
    int16_t strip_row1 = row_end - g_strip_y0;
    for (int16_t sr = strip_row0; sr < strip_row1; sr++) {
        uint16_t *p = &g_strip_buf[sr * DISPLAY_WIDTH + x];
        // Manual word fill (faster than memset for odd sizes; our w is small)
        for (int16_t col = 0; col < w; col++) {
            p[col] = color;
        }
    }
}

// ── Sprite (uint16_t data) ──────────────────────────────────

void graphics_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data) {
    // Clip to screen
    int16_t start_x = (x < 0) ? -x : 0;
    int16_t end_x = w;
    if (x + w > DISPLAY_WIDTH) end_x = DISPLAY_WIDTH - x;
    if (x >= DISPLAY_WIDTH || end_x <= start_x) return;

    // Clip to strip
    int16_t strip_end = g_strip_y0 + g_strip_h;
    int16_t row_start = y;
    if (row_start < g_strip_y0) row_start = g_strip_y0;
    int16_t row_end = y + h;
    if (row_end > strip_end) row_end = strip_end;
    if (row_start >= row_end) return;

    int16_t px_count = end_x - start_x;
    int16_t screen_x = x + start_x;

    for (int16_t row = row_start; row < row_end; row++) {
        int16_t src_row = row - y;
        const uint16_t *src = &data[src_row * w + start_x];
        uint16_t *dst = &g_strip_buf[(row - g_strip_y0) * DISPLAY_WIDTH + screen_x];
        memcpy(dst, src, px_count * sizeof(uint16_t));
    }
}

// ── Sprite from region map + palette ────────────────────────

void graphics_draw_sprite_region(int16_t x, int16_t y, int16_t w, int16_t h,
                                 const uint8_t *map, const uint16_t *palette) {
    // Clip to screen
    int16_t start_x = (x < 0) ? -x : 0;
    int16_t end_x = w;
    if (x + w > DISPLAY_WIDTH) end_x = DISPLAY_WIDTH - x;
    if (x >= DISPLAY_WIDTH || end_x <= start_x) return;

    // Clip to strip
    int16_t strip_end = g_strip_y0 + g_strip_h;
    int16_t row_start = y;
    if (row_start < g_strip_y0) row_start = g_strip_y0;
    int16_t row_end = y + h;
    if (row_end > strip_end) row_end = strip_end;
    if (row_start >= row_end) return;

    int16_t screen_x = x + start_x;
    int16_t px_count = end_x - start_x;

    for (int16_t row = row_start; row < row_end; row++) {
        int16_t src_row = row - y;
        const uint8_t *src = &map[src_row * w + start_x];
        uint16_t *dst = &g_strip_buf[(row - g_strip_y0) * DISPLAY_WIDTH + screen_x];
        for (int16_t col = 0; col < px_count; col++) {
            uint8_t region = src[col];
            if (region != 0) {
                dst[col] = palette[region];
            }
        }
    }
}

// ── Text rendering ──────────────────────────────────────────

int graphics_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (x >= DISPLAY_WIDTH || x + FONT_WIDTH < 0) {
        return x + FONT_WIDTH;
    }

    // Quick reject: entire character outside strip
    if (y + FONT_HEIGHT <= g_strip_y0 || y >= g_strip_y0 + g_strip_h) {
        return x + FONT_WIDTH;
    }

    const uint8_t *glyph = font_get_char(c);
    int16_t strip_end = g_strip_y0 + g_strip_h;

    for (int16_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = glyph[col];
        int16_t px = x + col;
        if (px < 0 || px >= DISPLAY_WIDTH) continue;

        for (int16_t row = 0; row < FONT_HEIGHT; row++) {
            int16_t py = y + row;
            if (py < g_strip_y0 || py >= strip_end) continue;
            uint16_t *p = &g_strip_buf[(py - g_strip_y0) * DISPLAY_WIDTH + px];
            *p = (line & (1 << row)) ? color : bg_color;
        }
    }

    return x + FONT_WIDTH;
}

int graphics_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width) {
    int16_t cur_x = x;
    int16_t cur_y = y;

    while (*text) {
        char c = *text;

        /* ---- 1. Explicit newline ---- */
        if (c == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT;
            text++;
            continue;
        }

        /* ---- 2. Word wrap: peek ahead at the next word on a space ---- */
        if (c == ' ' && max_width > 0) {
            const char *p = text + 1;
            int word_w = 0;
            while (*p && *p != ' ' && *p != '\n') {
                word_w += FONT_WIDTH;
                p++;
            }
            if (cur_x + word_w > x + max_width) {
                cur_x = x;
                cur_y += FONT_HEIGHT;
                text++;
                continue;
            }
        }

        /* ---- 3. Character-level line wrap ---- */
        if (max_width > 0 && cur_x + FONT_WIDTH > x + max_width) {
            cur_x = x;
            cur_y += FONT_HEIGHT;
        }

        /* ---- 4. Draw the current character ---- */
        cur_x = graphics_draw_char(cur_x, cur_y, c, color, bg_color);
        text++;
    }

    return cur_y + FONT_HEIGHT;
}

// ── Rainbow ─────────────────────────────────────────────────

static uint16_t hue_to_color(int hue) {
    int h = hue % 360;
    int sector = h / 60;
    int fract  = h - sector * 60;
    int p = 0;
    int q = (255 * (60 - fract) + 30) / 60;
    int t = (255 * fract + 30) / 60;
    int r, g, b;

    switch (sector) {
        case 0: r = 255; g = t;   b = p;   break;
        case 1: r = q;   g = 255; b = p;   break;
        case 2: r = p;   g = 255; b = t;   break;
        case 3: r = p;   g = q;   b = 255; break;
        case 4: r = t;   g = p;   b = 255; break;
        default: r = 255; g = p;   b = q;   break;
    }

    return COLOR(r, g, b);
}

// ── Rounded rectangle ──────────────────────────────────────

void graphics_draw_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t bg, uint16_t border) {
    int16_t r = 6;
    /* fill body */
    if (h > 2 * r) {
        graphics_fill_rect(x, y + r, w, h - 2 * r, bg);
    }
    if (w > 2 * r) {
        graphics_fill_rect(x + r, y, w - 2 * r, h, bg);
    }
    /* fill corner quadrants (approximate with circles) */
    for (int16_t dy = 0; dy <= r; dy++) {
        int dx_max = 0;
        for (int dx = r; dx >= 0; dx--) {
            if (dx * dx + dy * dy <= r * r) { dx_max = dx; break; }
        }
        if (dx_max > 0) {
            /* top-left */
            graphics_fill_rect(x + r - dx_max, y + r - dy, dx_max, 1, bg);
            /* top-right */
            graphics_fill_rect(x + w - r, y + r - dy, dx_max, 1, bg);
            /* bottom-left */
            graphics_fill_rect(x + r - dx_max, y + h - r + dy - 1, dx_max, 1, bg);
            /* bottom-right */
            graphics_fill_rect(x + w - r, y + h - r + dy - 1, dx_max, 1, bg);

            /* corner border arcs — outermost pixel of each quadrant */
            graphics_draw_pixel(x + r - dx_max, y + r - dy, border);
            graphics_draw_pixel(x + w - r + dx_max - 1, y + r - dy, border);
            graphics_draw_pixel(x + r - dx_max, y + h - r + dy - 1, border);
            graphics_draw_pixel(x + w - r + dx_max - 1, y + h - r + dy - 1, border);
        }
    }
    /* simple border via thin lines */
    graphics_fill_rect(x, y + r, 1, h - 2 * r, border);
    graphics_fill_rect(x + w - 1, y + r, 1, h - 2 * r, border);
    graphics_fill_rect(x + r, y, w - 2 * r, 1, border);
    graphics_fill_rect(x + r, y + h - 1, w - 2 * r, 1, border);
}

// ── Rainbow ─────────────────────────────────────────────────

void graphics_draw_rainbow_h(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    // Clip to strip
    int16_t strip_end = g_strip_y0 + g_strip_h;
    int16_t row_start = y;
    if (row_start < g_strip_y0) row_start = g_strip_y0;
    int16_t row_end = y + h;
    if (row_end > strip_end) row_end = strip_end;
    if (row_start >= row_end) return;

    int16_t denom = (w > 1) ? (w - 1) : 1;
    for (int16_t col = 0; col < w; col++) {
        int hue = (360 * col) / denom;
        uint16_t color = hue_to_color(hue);
        for (int16_t row = row_start; row < row_end; row++) {
            g_strip_buf[(row - g_strip_y0) * DISPLAY_WIDTH + x + col] = color;
        }
    }
}
