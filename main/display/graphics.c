#include "graphics.h"
#include "font.h"

// ── Internal helpers ──────────────────────────────────────────

static inline uint16_t *fb_pixel(int16_t x, int16_t y) {
    uint16_t *fb = display_get_framebuffer();
    return &fb[y * DISPLAY_WIDTH + x];
}

// ── Fill ──────────────────────────────────────────────────────

void graphics_fill(uint16_t color) {
    uint16_t *fb = display_get_framebuffer();
    size_t count = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    for (size_t i = 0; i < count; i++) {
        fb[i] = color;
    }
    display_mark_dirty();
}

// ── Pixel ─────────────────────────────────────────────────────

void graphics_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    *fb_pixel(x, y) = color;
    display_mark_dirty();
}

// ── Filled rectangle ──────────────────────────────────────────

void graphics_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    uint16_t *p_row = fb_pixel(x, y);
    for (int16_t row = 0; row < h; row++) {
        uint16_t *p = p_row;
        for (int16_t col = 0; col < w; col++, p++) {
            *p = color;
        }
        p_row += DISPLAY_WIDTH;
    }
    display_mark_dirty();
}

// ── Sprite ────────────────────────────────────────────────────

void graphics_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    int16_t start_x = (x < 0) ? -x : 0;
    int16_t start_y = (y < 0) ? -y : 0;
    int16_t end_x = (x + w > DISPLAY_WIDTH)  ? DISPLAY_WIDTH - x : w;
    int16_t end_y = (y + h > DISPLAY_HEIGHT) ? DISPLAY_HEIGHT - y : h;

    for (int16_t row = start_y; row < end_y; row++) {
        for (int16_t col = start_x; col < end_x; col++) {
            uint16_t color = data[row * w + col];
            *fb_pixel(x + col, y + row) = color;
        }
    }
    display_mark_dirty();
}

// ── Text rendering ────────────────────────────────────────────

int graphics_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || x + FONT_WIDTH < 0 || y + FONT_HEIGHT < 0) {
        return x + FONT_WIDTH;
    }

    const uint8_t *glyph = font_get_char(c);

    for (int16_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = glyph[col];
        int16_t px = x + col;
        if (px < 0 || px >= DISPLAY_WIDTH) continue;

        uint16_t *p = fb_pixel(px, y);
        for (int16_t row = 0; row < FONT_HEIGHT; row++, p += DISPLAY_WIDTH) {
            int16_t py = y + row;
            if (py >= 0 && py < DISPLAY_HEIGHT) {
                *p = (line & (1 << row)) ? color : bg_color;
            }
        }
    }

    display_mark_dirty();
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

// ── Rainbow ───────────────────────────────────────────────────

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

// ── Rounded rectangle ────────────────────────────────────────

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

// ── Rainbow ───────────────────────────────────────────────────

void graphics_draw_rainbow_h(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    int16_t denom = (w > 1) ? (w - 1) : 1;
    for (int16_t col = 0; col < w; col++) {
        int hue = (360 * col) / denom;
        uint16_t color = hue_to_color(hue);
        uint16_t *p = fb_pixel(x + col, y);
        for (int16_t row = 0; row < h; row++) {
            *p = color;
            p += DISPLAY_WIDTH;
        }
    }
    display_mark_dirty();
}
