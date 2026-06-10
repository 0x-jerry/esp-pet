/**
 * @file pet_sprite.c
 * @brief Procedural pet sprite rendered via gfx_img.
 *
 * Sprites are built into uint16_t pixel buffers and pushed to a gfx_img
 * widget.  Animation frames alternate based on elapsed time.
 */

#include "pet_sprite.h"
#include "display.h"
#include "display/gfx_helper.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#define SPR_W 48
#define SPR_H 48

/* ── Sprite frame builders ────────────────────────────────────── */

static void build_sprite_frame(uint16_t *buf, uint16_t body, uint16_t belly,
                                uint16_t ear_inner, uint16_t eye_color,
                                int mouth_y, int mouth_h) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 8, 6, 10, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 8, 6, 10, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 8, 3, 6, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 8, 3, 6, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 26, 16, 18, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 30, 10, 10, belly);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 13, 42, 6, 4, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 35, 42, 6, 4, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 18, 22, 3, 4, eye_color);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 30, 22, 3, 4, eye_color);
    gfx_buf_circle(buf, SPR_W, SPR_H, 19, 21, 1, GFX_COLOR_WHITE.full);
    gfx_buf_circle(buf, SPR_W, SPR_H, 31, 21, 1, GFX_COLOR_WHITE.full);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 27, 2, 2, RGB565(200, 100, 100));
    gfx_buf_rect(buf, SPR_W, SPR_H, 20, mouth_y, 8, mouth_h, GFX_COLOR_BLACK.full);
}

static void build_sprite_frame_alt(uint16_t *buf, uint16_t body,
                                    uint16_t belly, uint16_t ear_inner,
                                    uint16_t eye_color,
                                    int mouth_y, int mouth_h) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 10, 6, 8, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 10, 6, 8, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 10, 3, 5, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 10, 3, 5, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 27, 15, 16, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 31, 9, 9, belly);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 43, 6, 4, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 43, 6, 4, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 18, 23, 3, 4, eye_color);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 30, 23, 3, 4, eye_color);
    gfx_buf_circle(buf, SPR_W, SPR_H, 19, 22, 1, GFX_COLOR_WHITE.full);
    gfx_buf_circle(buf, SPR_W, SPR_H, 31, 22, 1, GFX_COLOR_WHITE.full);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 28, 2, 2, RGB565(200, 100, 100));
    gfx_buf_rect(buf, SPR_W, SPR_H, 20, mouth_y, 8, mouth_h, GFX_COLOR_BLACK.full);
}

static void build_sprite_frame_sleep(uint16_t *buf, uint16_t body,
                                      uint16_t belly, uint16_t ear_inner) {
    memset(buf, 0, SPR_W * SPR_H * 2);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 12, 6, 6, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 12, 6, 6, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 12, 12, 3, 4, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 36, 12, 3, 4, ear_inner);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 28, 17, 16, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 32, 10, 9, belly);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 18, 43, 7, 4, body);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 30, 43, 7, 4, body);
    gfx_buf_rect(buf, SPR_W, SPR_H, 15, 23, 6, 1, GFX_COLOR_BLACK.full);
    gfx_buf_rect(buf, SPR_W, SPR_H, 27, 23, 6, 1, GFX_COLOR_BLACK.full);
    gfx_buf_ellipse(buf, SPR_W, SPR_H, 24, 28, 2, 2, RGB565(200, 100, 100));
    gfx_buf_circle(buf, SPR_W, SPR_H, 24, 33, 2, GFX_COLOR_BLACK.full);
    gfx_buf_rect(buf, SPR_W, SPR_H, 36, 5, 1, 4, GFX_COLOR_WHITE.full);
    gfx_buf_rect(buf, SPR_W, SPR_H, 37, 5, 4, 1, GFX_COLOR_WHITE.full);
    gfx_buf_rect(buf, SPR_W, SPR_H, 36, 7, 4, 1, GFX_COLOR_WHITE.full);
    gfx_buf_rect(buf, SPR_W, SPR_H, 36, 9, 1, 4, GFX_COLOR_WHITE.full);
}

/* ── Mood -> color mapping ────────────────────────────────────── */

static void mood_colors(pet_mood_t mood,
                         uint16_t *body, uint16_t *belly,
                         uint16_t *ear_inner, uint16_t *eye) {
    switch (mood) {
    case PET_MOOD_HAPPY:
        *body = RGB565(255, 220, 100);
        *belly = RGB565(255, 245, 200);
        *ear_inner = RGB565(255, 180, 150);
        *eye = GFX_COLOR_BLACK.full;
        break;
    case PET_MOOD_SAD:
        *body = RGB565(200, 200, 150);
        *belly = RGB565(230, 230, 200);
        *ear_inner = RGB565(170, 170, 130);
        *eye = RGB565(60, 60, 80);
        break;
    case PET_MOOD_HUNGRY:
        *body = RGB565(255, 180, 60);
        *belly = RGB565(255, 220, 160);
        *ear_inner = RGB565(255, 140, 100);
        *eye = GFX_COLOR_BLACK.full;
        break;
    case PET_MOOD_SLEEPY:
        *body = RGB565(190, 190, 130);
        *belly = RGB565(220, 220, 180);
        *ear_inner = RGB565(160, 160, 120);
        *eye = RGB565(80, 80, 80);
        break;
    default:
        *body = RGB565(240, 210, 110);
        *belly = RGB565(255, 245, 200);
        *ear_inner = RGB565(240, 170, 140);
        *eye = GFX_COLOR_BLACK.full;
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