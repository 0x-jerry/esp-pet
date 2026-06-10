/**
 * @file gfx_helper.c
 * @brief Implementation of gfx pixel-buffer helpers and widget factories.
 */

#include "gfx_helper.h"
#include <string.h>
#include <stdlib.h>

/* ── Colour constants ─────────────────────────────────────────── */

const gfx_color_t GFX_COLOR_WHITE      = { .full = 0xFFFF };
const gfx_color_t GFX_COLOR_RED        = { .full = 0xF800 };
const gfx_color_t GFX_COLOR_GREEN      = { .full = 0x07E0 };
const gfx_color_t GFX_COLOR_BLUE       = { .full = 0x001F };
const gfx_color_t GFX_COLOR_YELLOW     = { .full = 0xFFE0 };
const gfx_color_t GFX_COLOR_CYAN       = { .full = 0x07FF };
const gfx_color_t GFX_COLOR_GRAY       = { .full = 0x8410 };
const gfx_color_t GFX_COLOR_DARK_GRAY  = { .full = 0x4208 };

/* ── Buffer fill ──────────────────────────────────────────────── */

void gfx_buf_fill(uint16_t *buf, int w, int h, uint16_t color) {
    int n = w * h;
    for (int i = 0; i < n; i++) buf[i] = color;
}

/* ── Rainbow gradient ─────────────────────────────────────────── */

void gfx_buf_rainbow_h(uint16_t *buf, int w, int h) {
    for (int x = 0; x < w; x++) {
        int hue = (x * 360) / (w > 1 ? w - 1 : 1);
        int sector = hue / 60;
        int fract  = hue - sector * 60;
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
        uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        for (int y = 0; y < h; y++) buf[y * w + x] = c;
    }
}

/* ── Image descriptor helpers ─────────────────────────────────── */

void gfx_init_dsc(gfx_image_dsc_t *dsc, uint16_t *buf, int w, int h) {
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic  = 0x19;
    dsc->header.cf     = GFX_COLOR_FORMAT_RGB565;
    dsc->header.w      = w;
    dsc->header.h      = h;
    dsc->header.stride = w;
    dsc->data_size     = w * h * 2;
    dsc->data          = (const uint8_t *)buf;
}

void gfx_make_solid_dsc(gfx_image_dsc_t *dsc, uint16_t *buf,
                        int w, int h, uint16_t color) {
    gfx_init_dsc(dsc, buf, w, h);
    gfx_buf_fill(buf, w, h, color);
}

/* ── Solid-colour rect image widget ───────────────────────────── */

gfx_obj_t *gfx_make_rect_img(gfx_disp_t *disp, int x, int y,
                              int w, int h, uint16_t color,
                              uint16_t **out_buf) {
    uint16_t *buf = calloc(1, w * h * 2);
    if (!buf) return NULL;
    gfx_image_dsc_t dsc;
    gfx_make_solid_dsc(&dsc, buf, w, h, color);
    gfx_obj_t *img = gfx_img_create(disp);
    if (!img) { free(buf); return NULL; }
    gfx_img_set_src(img, (void *)&dsc);
    gfx_obj_set_pos(img, x, y);
    if (out_buf) *out_buf = buf;
    return img;
}
