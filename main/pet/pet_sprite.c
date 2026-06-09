#include "pet_sprite.h"
#include "graphics.h"
#include "esp_timer.h"
#include <string.h>

/* ── Simple 48×48 pixel art sprites (row-major uint16_t) ─── */

/*
 * Frame 0: neutral/standing — round body, 2 ears, 2 feet, face blob
 * Each byte pair = RGB565 pixel. Built procedurally in code for brevity.
 */
#define SPR_W 48
#define SPR_H 48

/*
 * Draw a filled circle into a sprite buffer.
 */
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

static void buf_ellipse(uint16_t *buf, int cx, int cy, int rx, int ry, uint16_t color) {
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

static void buf_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                buf[py * SPR_W + px] = color;
            }
        }
    }
}

/* ── Build a sprite frame (body color variant) ───────────── */

static void build_sprite_frame(uint16_t *buf, uint16_t body, uint16_t belly,
                                uint16_t ear_inner, uint16_t eye_color,
                                int mouth_y, int mouth_h) {
    /* clear */
    memset(buf, 0, SPR_W * SPR_H * 2);

    /* ears — two triangles on top */
    buf_ellipse(buf, 12, 8, 6, 10, body);          /* left ear */
    buf_ellipse(buf, 36, 8, 6, 10, body);          /* right ear */
    buf_ellipse(buf, 12, 8, 3, 6, ear_inner);       /* left ear inner */
    buf_ellipse(buf, 36, 8, 3, 6, ear_inner);       /* right ear inner */

    /* body — large ellipse */
    buf_ellipse(buf, 24, 26, 16, 18, body);

    /* belly patch */
    buf_ellipse(buf, 24, 30, 10, 10, belly);

    /* feet */
    buf_ellipse(buf, 13, 42, 6, 4, body);
    buf_ellipse(buf, 35, 42, 6, 4, body);

    /* eyes */
    buf_ellipse(buf, 18, 22, 3, 4, eye_color);
    buf_ellipse(buf, 30, 22, 3, 4, eye_color);

    /* eye highlights */
    buf_circle(buf, 19, 21, 1, COLOR_WHITE);
    buf_circle(buf, 31, 21, 1, COLOR_WHITE);

    /* nose */
    buf_ellipse(buf, 24, 27, 2, 2, COLOR(200, 100, 100));

    /* mouth */
    buf_rect(buf, 20, mouth_y, 8, mouth_h, COLOR_BLACK);
}

/*
 * Frame 1: slight bob — body a tiny bit shorter, ears lowered
 */
static void build_sprite_frame_alt(uint16_t *buf, uint16_t body, uint16_t belly,
                                    uint16_t ear_inner, uint16_t eye_color,
                                    int mouth_y, int mouth_h) {
    memset(buf, 0, SPR_W * SPR_H * 2);

    /* ears slightly lower */
    buf_ellipse(buf, 12, 10, 6, 8, body);
    buf_ellipse(buf, 36, 10, 6, 8, body);
    buf_ellipse(buf, 12, 10, 3, 5, ear_inner);
    buf_ellipse(buf, 36, 10, 3, 5, ear_inner);

    /* body slightly shorter */
    buf_ellipse(buf, 24, 27, 15, 16, body);
    buf_ellipse(buf, 24, 31, 9, 9, belly);

    /* feet slightly spread */
    buf_ellipse(buf, 12, 43, 6, 4, body);
    buf_ellipse(buf, 36, 43, 6, 4, body);

    /* eyes */
    buf_ellipse(buf, 18, 23, 3, 4, eye_color);
    buf_ellipse(buf, 30, 23, 3, 4, eye_color);
    buf_circle(buf, 19, 22, 1, COLOR_WHITE);
    buf_circle(buf, 31, 22, 1, COLOR_WHITE);

    /* nose */
    buf_ellipse(buf, 24, 28, 2, 2, COLOR(200, 100, 100));

    buf_rect(buf, 20, mouth_y, 8, mouth_h, COLOR_BLACK);
}

/*
 * Sleeping frame: closed eyes, zZz bubbles
 */
static void build_sprite_frame_sleep(uint16_t *buf, uint16_t body, uint16_t belly,
                                      uint16_t ear_inner) {
    memset(buf, 0, SPR_W * SPR_H * 2);

    /* ears drooped low */
    buf_ellipse(buf, 12, 12, 6, 6, body);
    buf_ellipse(buf, 36, 12, 6, 6, body);
    buf_ellipse(buf, 12, 12, 3, 4, ear_inner);
    buf_ellipse(buf, 36, 12, 3, 4, ear_inner);

    /* body — slightly wider, lower */
    buf_ellipse(buf, 24, 28, 17, 16, body);
    buf_ellipse(buf, 24, 32, 10, 9, belly);

    /* feet together */
    buf_ellipse(buf, 18, 43, 7, 4, body);
    buf_ellipse(buf, 30, 43, 7, 4, body);

    /* closed eyes — horizontal lines */
    buf_rect(buf, 15, 23, 6, 1, COLOR_BLACK);
    buf_rect(buf, 27, 23, 6, 1, COLOR_BLACK);

    /* nose */
    buf_ellipse(buf, 24, 28, 2, 2, COLOR(200, 100, 100));

    /* tiny 'o' mouth */
    buf_circle(buf, 24, 33, 2, COLOR_BLACK);

    /* zZz */
    buf_rect(buf, 36, 5, 1, 4, COLOR_WHITE);
    buf_rect(buf, 37, 5, 4, 1, COLOR_WHITE);
    buf_rect(buf, 36, 7, 4, 1, COLOR_WHITE);
    buf_rect(buf, 36, 9, 1, 4, COLOR_WHITE);
}

/* ── Mood → color mapping ────────────────────────────────── */

static void mood_colors(pet_mood_t mood,
                         uint16_t *body, uint16_t *belly,
                         uint16_t *ear_inner, uint16_t *eye) {
    switch (mood) {
    case PET_MOOD_HAPPY:
        *body       = COLOR(255, 220, 100);
        *belly      = COLOR(255, 245, 200);
        *ear_inner  = COLOR(255, 180, 150);
        *eye        = COLOR_BLACK;
        break;
    case PET_MOOD_SAD:
        *body       = COLOR(200, 200, 150);
        *belly      = COLOR(230, 230, 200);
        *ear_inner  = COLOR(170, 170, 130);
        *eye        = COLOR(60, 60, 80);
        break;
    case PET_MOOD_HUNGRY:
        *body       = COLOR(255, 180, 60);
        *belly      = COLOR(255, 220, 160);
        *ear_inner  = COLOR(255, 140, 100);
        *eye        = COLOR_BLACK;
        break;
    case PET_MOOD_SLEEPY:
        *body       = COLOR(190, 190, 130);
        *belly      = COLOR(220, 220, 180);
        *ear_inner  = COLOR(160, 160, 120);
        *eye        = COLOR(80, 80, 80);
        break;
    default: /* neutral */
        *body       = COLOR(240, 210, 110);
        *belly      = COLOR(255, 245, 200);
        *ear_inner  = COLOR(240, 170, 140);
        *eye        = COLOR_BLACK;
        break;
    }
}

/* ── Sprite cache ────────────────────────────────────────── */

static uint16_t g_sprite_frames[PET_SPRITE_FRAMES][SPR_W * SPR_H];
static uint16_t g_sprite_sleep[SPR_W * SPR_H];
static uint8_t  g_current_frame;
static int64_t  g_last_anim_ms;
static pet_mood_t g_cached_mood = PET_MOOD_COUNT;
static bool     g_cached_sleep = false;

/* ── Public API ──────────────────────────────────────────── */

void pet_sprite_reset_animation(void) {
    g_cached_mood = PET_MOOD_COUNT;
    g_cached_sleep = false;
    g_last_anim_ms = 0;
}

void pet_sprite_draw(int16_t x, int16_t y, pet_mood_t mood, bool sleeping) {
    int64_t now_ms = esp_timer_get_time() / 1000;

    /* rebuild sprite buffer if mood/sleep changed */
    if (mood != g_cached_mood || sleeping != g_cached_sleep || g_last_anim_ms == 0) {
        uint16_t body, belly, ear_inner, eye;
        mood_colors(mood, &body, &belly, &ear_inner, &eye);

        if (sleeping) {
            build_sprite_frame_sleep(g_sprite_sleep, body, belly, ear_inner);
        } else {
            build_sprite_frame(g_sprite_frames[0], body, belly, ear_inner, eye, 32, 3);
            build_sprite_frame_alt(g_sprite_frames[1], body, belly, ear_inner, eye, 33, 2);
        }

        g_cached_mood = mood;
        g_cached_sleep = sleeping;
        g_current_frame = 0;
        g_last_anim_ms = now_ms;
    }

    /* advance animation */
    if (!sleeping && (now_ms - g_last_anim_ms >= PET_ANIM_INTERVAL_MS)) {
        g_current_frame = (g_current_frame + 1) % PET_SPRITE_FRAMES;
        g_last_anim_ms = now_ms;
    }

    /* draw current frame */
    const uint16_t *data = sleeping ? g_sprite_sleep : g_sprite_frames[g_current_frame];
    graphics_draw_sprite(x, y, SPR_W, SPR_H, data);
}
