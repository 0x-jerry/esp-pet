#include "pet_sprite.h"
#include "graphics.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "pet_sprite";

#define SPR_W 48
#define SPR_H 48

/* ── Region IDs (0 = transparent, skipped during render) ─── */

enum {
    R_NONE      = 0,
    R_BODY      = 1,
    R_BELLY     = 2,
    R_EAR_INNER = 3,
    R_EYE       = 4,
    R_EYE_HL    = 5,
    R_NOSE      = 6,
    R_MOUTH     = 7,
    R_ZZZ       = 8,
    R_COUNT     = 9,
};

/* ── Region map storage ───────────────────────────────────── */

// 3 region maps: idle frame 0, bob frame 1, sleep frame
// Each is 48×48 uint8_t, allocated in SPIRAM
static uint8_t *g_regions_idle[2];   // [0] = frame 0, [1] = frame 1 (bob)
static uint8_t *g_regions_sleep;     // sleep frame

/* ── Mood → palette (region ID → RGB565 color) ────────────── */

static const uint16_t g_palettes[PET_MOOD_COUNT][R_COUNT] = {
    [PET_MOOD_HAPPY] = {
        [R_BODY]      = COLOR(255, 220, 100),
        [R_BELLY]     = COLOR(255, 245, 200),
        [R_EAR_INNER] = COLOR(255, 180, 150),
        [R_EYE]       = COLOR_BLACK,
        [R_EYE_HL]    = COLOR_WHITE,
        [R_NOSE]      = COLOR(200, 100, 100),
        [R_MOUTH]     = COLOR_BLACK,
        [R_ZZZ]       = COLOR_WHITE,
    },
    [PET_MOOD_NEUTRAL] = {
        [R_BODY]      = COLOR(240, 210, 110),
        [R_BELLY]     = COLOR(255, 245, 200),
        [R_EAR_INNER] = COLOR(240, 170, 140),
        [R_EYE]       = COLOR_BLACK,
        [R_EYE_HL]    = COLOR_WHITE,
        [R_NOSE]      = COLOR(200, 100, 100),
        [R_MOUTH]     = COLOR_BLACK,
        [R_ZZZ]       = COLOR_WHITE,
    },
    [PET_MOOD_SAD] = {
        [R_BODY]      = COLOR(200, 200, 150),
        [R_BELLY]     = COLOR(230, 230, 200),
        [R_EAR_INNER] = COLOR(170, 170, 130),
        [R_EYE]       = COLOR(60, 60, 80),
        [R_EYE_HL]    = COLOR_WHITE,
        [R_NOSE]      = COLOR(200, 100, 100),
        [R_MOUTH]     = COLOR_BLACK,
        [R_ZZZ]       = COLOR_WHITE,
    },
    [PET_MOOD_HUNGRY] = {
        [R_BODY]      = COLOR(255, 180, 60),
        [R_BELLY]     = COLOR(255, 220, 160),
        [R_EAR_INNER] = COLOR(255, 140, 100),
        [R_EYE]       = COLOR_BLACK,
        [R_EYE_HL]    = COLOR_WHITE,
        [R_NOSE]      = COLOR(200, 100, 100),
        [R_MOUTH]     = COLOR_BLACK,
        [R_ZZZ]       = COLOR_WHITE,
    },
    [PET_MOOD_SLEEPY] = {
        [R_BODY]      = COLOR(190, 190, 130),
        [R_BELLY]     = COLOR(220, 220, 180),
        [R_EAR_INNER] = COLOR(160, 160, 120),
        [R_EYE]       = COLOR(80, 80, 80),
        [R_EYE_HL]    = COLOR_WHITE,
        [R_NOSE]      = COLOR(200, 100, 100),
        [R_MOUTH]     = COLOR_BLACK,
        [R_ZZZ]       = COLOR_WHITE,
    },
};

/* ── Region map drawing helpers (write uint8_t region IDs) ── */

static void region_circle(uint8_t *map, int cx, int cy, int r, uint8_t rid) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                    map[py * SPR_W + px] = rid;
                }
            }
        }
    }
}

static void region_ellipse(uint8_t *map, int cx, int cy, int rx, int ry, uint8_t rid) {
    for (int dy = -ry; dy <= ry; dy++) {
        for (int dx = -rx; dx <= rx; dx++) {
            if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                    map[py * SPR_W + px] = rid;
                }
            }
        }
    }
}

static void region_rect(uint8_t *map, int x, int y, int w, int h, uint8_t rid) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < SPR_W && py >= 0 && py < SPR_H) {
                map[py * SPR_W + px] = rid;
            }
        }
    }
}

/* ── Region map generators (called once at init) ──────────── */

/*
 * Frame 0: neutral/standing — round body, 2 ears, 2 feet, face
 */
static void gen_region_map_frame0(uint8_t *map) {
    memset(map, 0, SPR_W * SPR_H);

    /* ears */
    region_ellipse(map, 12, 8, 6, 10, R_BODY);
    region_ellipse(map, 36, 8, 6, 10, R_BODY);
    region_ellipse(map, 12, 8, 3, 6, R_EAR_INNER);
    region_ellipse(map, 36, 8, 3, 6, R_EAR_INNER);

    /* body */
    region_ellipse(map, 24, 26, 16, 18, R_BODY);
    region_ellipse(map, 24, 30, 10, 10, R_BELLY);

    /* feet */
    region_ellipse(map, 13, 42, 6, 4, R_BODY);
    region_ellipse(map, 35, 42, 6, 4, R_BODY);

    /* eyes */
    region_ellipse(map, 18, 22, 3, 4, R_EYE);
    region_ellipse(map, 30, 22, 3, 4, R_EYE);
    /* eye highlights */
    region_circle(map, 19, 21, 1, R_EYE_HL);
    region_circle(map, 31, 21, 1, R_EYE_HL);

    /* nose */
    region_ellipse(map, 24, 27, 2, 2, R_NOSE);

    /* mouth */
    region_rect(map, 20, 32, 8, 3, R_MOUTH);
}

/*
 * Frame 1: slight bob — body shorter, ears lowered
 */
static void gen_region_map_frame1(uint8_t *map) {
    memset(map, 0, SPR_W * SPR_H);

    /* ears slightly lower */
    region_ellipse(map, 12, 10, 6, 8, R_BODY);
    region_ellipse(map, 36, 10, 6, 8, R_BODY);
    region_ellipse(map, 12, 10, 3, 5, R_EAR_INNER);
    region_ellipse(map, 36, 10, 3, 5, R_EAR_INNER);

    /* body slightly shorter */
    region_ellipse(map, 24, 27, 15, 16, R_BODY);
    region_ellipse(map, 24, 31, 9, 9, R_BELLY);

    /* feet slightly spread */
    region_ellipse(map, 12, 43, 6, 4, R_BODY);
    region_ellipse(map, 36, 43, 6, 4, R_BODY);

    /* eyes */
    region_ellipse(map, 18, 23, 3, 4, R_EYE);
    region_ellipse(map, 30, 23, 3, 4, R_EYE);
    region_circle(map, 19, 22, 1, R_EYE_HL);
    region_circle(map, 31, 22, 1, R_EYE_HL);

    /* nose */
    region_ellipse(map, 24, 28, 2, 2, R_NOSE);

    /* mouth */
    region_rect(map, 20, 33, 8, 2, R_MOUTH);
}

/*
 * Sleep frame: closed eyes, zZz bubbles
 */
static void gen_region_map_sleep(uint8_t *map) {
    memset(map, 0, SPR_W * SPR_H);

    /* ears drooped low */
    region_ellipse(map, 12, 12, 6, 6, R_BODY);
    region_ellipse(map, 36, 12, 6, 6, R_BODY);
    region_ellipse(map, 12, 12, 3, 4, R_EAR_INNER);
    region_ellipse(map, 36, 12, 3, 4, R_EAR_INNER);

    /* body — slightly wider, lower */
    region_ellipse(map, 24, 28, 17, 16, R_BODY);
    region_ellipse(map, 24, 32, 10, 9, R_BELLY);

    /* feet together */
    region_ellipse(map, 18, 43, 7, 4, R_BODY);
    region_ellipse(map, 30, 43, 7, 4, R_BODY);

    /* closed eyes — horizontal lines (use R_EYE region, palette handles color) */
    region_rect(map, 15, 23, 6, 1, R_EYE);
    region_rect(map, 27, 23, 6, 1, R_EYE);

    /* nose */
    region_ellipse(map, 24, 28, 2, 2, R_NOSE);

    /* tiny 'o' mouth */
    region_circle(map, 24, 33, 2, R_MOUTH);

    /* zZz */
    region_rect(map, 36, 5, 1, 4, R_ZZZ);
    region_rect(map, 37, 5, 4, 1, R_ZZZ);
    region_rect(map, 36, 7, 4, 1, R_ZZZ);
    region_rect(map, 36, 9, 1, 4, R_ZZZ);
}

/* ── Init: generate all region maps ───────────────────────── */

void pet_sprite_gen_regions(void) {
    ESP_LOGI(TAG, "Generating sprite region maps...");

    size_t map_size = SPR_W * SPR_H; /* uint8_t, so exactly 2304 bytes each */

    /* Allocate in SPIRAM (preferred) or DMA-capable SRAM */
    for (int i = 0; i < 2; i++) {
        g_regions_idle[i] = heap_caps_malloc(map_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!g_regions_idle[i]) {
            g_regions_idle[i] = heap_caps_malloc(map_size, MALLOC_CAP_8BIT);
        }
        assert(g_regions_idle[i]);
    }
    g_regions_sleep = heap_caps_malloc(map_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_regions_sleep) {
        g_regions_sleep = heap_caps_malloc(map_size, MALLOC_CAP_8BIT);
    }
    assert(g_regions_sleep);

    gen_region_map_frame0(g_regions_idle[0]);
    gen_region_map_frame1(g_regions_idle[1]);
    gen_region_map_sleep(g_regions_sleep);

    ESP_LOGI(TAG, "Region maps ready (%zu bytes total)", map_size * 3);
}

/* ── Animation state ──────────────────────────────────────── */

static uint8_t  g_current_frame;
static int64_t  g_last_anim_ms;

/* ── Public API ───────────────────────────────────────────── */

void pet_sprite_reset_animation(void) {
    g_last_anim_ms = 0;
}

void pet_sprite_draw(int16_t x, int16_t y, pet_mood_t mood, bool sleeping) {
    int64_t now_ms = esp_timer_get_time() / 1000;

    /* Reset animation clock on first call */
    if (g_last_anim_ms == 0) {
        g_last_anim_ms = now_ms;
        g_current_frame = 0;
    }

    /* Advance idle animation */
    if (!sleeping && (now_ms - g_last_anim_ms >= PET_ANIM_INTERVAL_MS)) {
        g_current_frame = (g_current_frame + 1) % PET_SPRITE_FRAMES;
        g_last_anim_ms = now_ms;
    }

    /* Select region map and palette */
    const uint8_t *map = sleeping ? g_regions_sleep : g_regions_idle[g_current_frame];
    const uint16_t *palette = g_palettes[mood];

    graphics_draw_sprite_region(x, y, SPR_W, SPR_H, map, palette);
}
