/**
 * @file pet_sprite.c
 * @brief Procedural pet sprite rendered via gfx_img.
 *
 * Sprites are built into uint16_t pixel buffers and pushed to a gfx_img
 * widget.  Animation frames alternate based on elapsed time.
 */

#include "pet_sprite.h"
#include "display/gfx_helper.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#define SPR_W 48
#define SPR_H 48

/* ── Colour helpers ───────────────────────────────────────────── */

#define COLOR(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define COL_BLACK     COLOR(0, 0, 0)
#define COL_WHITE     COLOR(255, 255, 255)

/* ── Buffer drawing primitives ────────────────────────────────── */

static void buf_circle(uint16_t *buf, int cx, int cy, int r, uint16_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                    buf[py * SPR_W + px] = color;
                }
            }
        }
    }
}

static void buf_ellipse(uint16_t *buf, int cx, int cy, int rx, int ry,
                        uint16_t color) {
    for (int dy = -ry; dy <= ry; dy++) {
        for (int dx = -rx; dx <= rx; dx++) {
            if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                    buf[py * SPR_W + px] = color;
                }
            }
        }
    }
}

static void buf_rect(uint16_t *buf, int x, int y, int w, int h,
                     uint16_t color) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                buf[py * SPR_W + px] = color;
            }
        }
    }
}

/* ── Sprite frame builders ────────────────────────────────────── */

static void build_sprite_frame(uint16_t *buf, uint16_t body, uint16_t belly,
                                uint16_t ear_inner, uint16_t eye_color,
                                int mouth_y, int mouth_h) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    buf_ellipse(buf, 12, 8, 6, 10, body);
    buf_ellipse(buf, 36, 8, 6, 10, body);
    buf_ellipse(buf, 12, 8, 3, 6, ear_inner);
    buf_ellipse(buf, 36, 8, 3, 6, ear_inner);
    buf_ellipse(buf, 24, 26, 16, 18, body);
    buf_ellipse(buf, 24, 30, 10, 10, belly);
    buf_ellipse(buf, 13, 42, 6, 4, body);
    buf_ellipse(buf, 35, 42, 6, 4, body);
    buf_ellipse(buf, 18, 22, 3, 4, eye_color);
    buf_ellipse(buf, 30, 22, 3, 4, eye_color);
    buf_circle(buf, 19, 21, 1, COL_WHITE);
    buf_circle(buf, 31, 21, 1, COL_WHITE);
    buf_ellipse(buf, 24, 27, 2, 2, COLOR(200, 100, 100));
    buf_rect(buf, 20, mouth_y, 8, mouth_h, COL_BLACK);
}

static void build_sprite_frame_alt(uint16_t *buf, uint16_t body,
                                    uint16_t belly, uint16_t ear_inner,
                                    uint16_t eye_color,
                                    int mouth_y, int mouth_h) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    buf_ellipse(buf, 12, 10, 6, 8, body);
    buf_ellipse(buf, 36, 10, 6, 8, body);
    buf_ellipse(buf, 12, 10, 3, 5, ear_inner);
    buf_ellipse(buf, 36, 10, 3, 5, ear_inner);
    buf_ellipse(buf, 24, 27, 15, 16, body);
    buf_ellipse(buf, 24, 31, 9, 9, belly);
    buf_ellipse(buf, 12, 43, 6, 4, body);
    buf_ellipse(buf, 36, 43, 6, 4, body);
    buf_ellipse(buf, 18, 23, 3, 4, eye_color);
    buf_ellipse(buf, 30, 23, 3, 4, eye_color);
    buf_circle(buf, 19, 22, 1, COL_WHITE);
    buf_circle(buf, 31, 22, 1, COL_WHITE);
    buf_ellipse(buf, 24, 28, 2, 2, COLOR(200, 100, 100));
    buf_rect(buf, 20, mouth_y, 8, mouth_h, COL_BLACK);
}

static void build_sprite_frame_sleep(uint16_t *buf, uint16_t body,
                                      uint16_t belly, uint16_t ear_inner) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    buf_ellipse(buf, 12, 12, 6, 6, body);
    buf_ellipse(buf, 36, 12, 6, 6, body);
    buf_ellipse(buf, 12, 12, 3, 4, ear_inner);
    buf_ellipse(buf, 36, 12, 3, 4, ear_inner);
    buf_ellipse(buf, 24, 28, 17, 16, body);
    buf_ellipse(buf, 24, 32, 10, 9, belly);
    buf_ellipse(buf, 18, 43, 7, 4, body);
    buf_ellipse(buf, 30, 43, 7, 4, body);
    buf_rect(buf, 15, 23, 6, 1, COL_BLACK);
    buf_rect(buf, 27, 23, 6, 1, COL_BLACK);
    buf_ellipse(buf, 24, 28, 2, 2, COLOR(200, 100, 100));
    buf_circle(buf, 24, 33, 2, COL_BLACK);
    buf_rect(buf, 36, 5, 1, 4, COL_WHITE);
    buf_rect(buf, 37, 5, 4, 1, COL_WHITE);
    buf_rect(buf, 36, 7, 4, 1, COL_WHITE);
    buf_rect(buf, 36, 9, 1, 4, COL_WHITE);
}

/* ── Mood -> color mapping ────────────────────────────────────── */

static void mood_colors(pet_mood_t mood,
                         uint16_t *body, uint16_t *belly,
                         uint16_t *ear_inner, uint16_t *eye) {
    switch (mood) {
    case PET_MOOD_HAPPY:
        *body = COLOR(255, 220, 100);
        *belly = COLOR(255, 245, 200);
        *ear_inner = COLOR(255, 180, 150);
        *eye = COL_BLACK;
        break;
    case PET_MOOD_SAD:
        *body = COLOR(200, 200, 150);
        *belly = COLOR(230, 230, 200);
        *ear_inner = COLOR(170, 170, 130);
        *eye = COLOR(60, 60, 80);
        break;
    case PET_MOOD_HUNGRY:
        *body = COLOR(255, 180, 60);
        *belly = COLOR(255, 220, 160);
        *ear_inner = COLOR(255, 140, 100);
        *eye = COL_BLACK;
        break;
    case PET_MOOD_SLEEPY:
        *body = COLOR(190, 190, 130);
        *belly = COLOR(220, 220, 180);
        *ear_inner = COLOR(160, 160, 120);
        *eye = COLOR(80, 80, 80);
        break;
    default:
        *body = COLOR(240, 210, 110);
        *belly = COLOR(255, 245, 200);
        *ear_inner = COLOR(240, 170, 140);
        *eye = COL_BLACK;
        break;
    }
}

/* ── gfx widget state ─────────────────────────────────────────── */

static gfx_obj_t      *g_sprite_img = NULL;
static gfx_image_dsc_t g_sprite_dsc;

/* pre-allocated sprite buffers (3 frames: idle-0, idle-1, sleep) */
static uint16_t g_sprite_buf_0[SPR_W * SPR_H];
static uint16_t g_sprite_buf_1[SPR_W * SPR_H];
static uint16_t g_sprite_buf_sleep[SPR_W * SPR_H];

/* animation state */
static int        g_current_frame = 0;
static int64_t    g_last_anim_ms = 0;
static pet_mood_t g_cached_mood  = PET_MOOD_COUNT;
static bool       g_cached_sleep = false;

/* ── Public API ───────────────────────────────────────────────── */

void pet_sprite_init(gfx_disp_t *disp) {
    g_sprite_img = gfx_img_create(disp);
    assert(g_sprite_img);

    gfx_init_dsc(&g_sprite_dsc, g_sprite_buf_0, SPR_W, SPR_H);

    gfx_obj_set_pos(g_sprite_img, 96, 72);
}

void pet_sprite_update(pet_mood_t mood, bool sleeping) {
    int64_t now_ms = esp_timer_get_time() / 1000;

    /* rebuild sprite buffers if mood/sleep changed */
    if (mood != g_cached_mood || sleeping != g_cached_sleep
        || g_last_anim_ms == 0) {
        uint16_t body, belly, ear_inner, eye;
        mood_colors(mood, &body, &belly, &ear_inner, &eye);

        if (sleeping) {
            build_sprite_frame_sleep(g_sprite_buf_sleep, body, belly,
                                     ear_inner);
        } else {
            build_sprite_frame(g_sprite_buf_0, body, belly, ear_inner,
                               eye, 32, 3);
            build_sprite_frame_alt(g_sprite_buf_1, body, belly, ear_inner,
                                   eye, 33, 2);
        }
        g_cached_mood  = mood;
        g_cached_sleep = sleeping;
        g_current_frame = 0;
        g_last_anim_ms = now_ms;
    }

    /* advance animation */
    if (!sleeping && (now_ms - g_last_anim_ms >= PET_ANIM_INTERVAL_MS)) {
        g_current_frame = (g_current_frame + 1) % PET_SPRITE_FRAMES;
        g_last_anim_ms = now_ms;
    }

    /* select buffer and push to gfx_img */
    const uint16_t *data = sleeping
        ? g_sprite_buf_sleep
        : (g_current_frame == 0 ? g_sprite_buf_0 : g_sprite_buf_1);

    g_sprite_dsc.data = (const uint8_t *)data;
    gfx_img_set_src(g_sprite_img, (void *)&g_sprite_dsc);
}