/**
 * @file ui.c
 * @brief UI rendering with esp_emote_gfx widgets.
 *
 * Uses gfx_label for text and gfx_img for coloured rectangles/stat bars.
 * All widgets are created in ui_init(); dynamic updates modify widget
 * properties (text, colour, image buffer) without recreating objects.
 */

#include "ui.h"
#include "display/gfx_helper.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "ui";

/* ── Global display reference ─────────────────────────────────── */
static gfx_disp_t *g_disp = NULL;

/* ── Widget handles ───────────────────────────────────────────── */

/* static */
static gfx_obj_t *w_title;
static gfx_obj_t *w_underline;
static gfx_obj_t *w_mood_bg;
static gfx_obj_t *w_hint;
static gfx_obj_t *w_ctrl;
static gfx_obj_t *w_mood;
static gfx_obj_t *w_care_hint;
static gfx_obj_t *w_rainbow;

/* stat bars (gfx_img backed by mutable buffers) */
static struct {
    gfx_obj_t     *obj;
    uint16_t      *buf;       /**< pixel buffer (max bar width × bar height) */
    gfx_image_dsc_t dsc;      /**< image descriptor pointing to buf */
    int16_t         x;
    int16_t         full_w;   /**< full bar width in pixels */
} g_bar[3];

/* gamepad debug labels */
static gfx_obj_t *w_dbg_title;
static gfx_obj_t *w_dbg_ls, *w_dbg_rs, *w_dbg_trig;
static gfx_obj_t *w_dbg_btns1, *w_dbg_btns2;
static bool       g_dbg_visible;

/* speech bubble */
static gfx_obj_t *w_bubble_bg;
static gfx_obj_t *w_bubble_text;
static uint16_t  *g_bubble_buf;
static gfx_image_dsc_t g_bubble_dsc;

/* ── Static draw ──────────────────────────────────────────────── */

void ui_draw_static(void) {
    char title[48];
    snprintf(title, sizeof(title), "ESP-PET — %s", pet_get_name());
    gfx_label_set_text(w_title, title);

    gfx_label_set_text(w_hint,
        "A:action B:talk L/R:cycle");

    gfx_label_set_text(w_ctrl, "Ctrl: searching...");
    gfx_label_set_color(w_ctrl, GFX_COLOR_GRAY);
}


/* ── Initialization ───────────────────────────────────────────── */

void ui_init(gfx_disp_t *disp) {
    g_disp = disp;

    /* -- title -- */
    w_title = gfx_label_create(disp);
    gfx_obj_set_pos(w_title, 10, UI_TITLE_Y);
    gfx_label_set_color(w_title, GFX_COLOR_WHITE);

    /* -- underline -- */
    w_underline = gfx_make_rect_img(disp, 10, UI_UNDERLINE_Y,
                                 220, 2, GFX_COLOR_WHITE.full, NULL);

    /* -- mood bar background -- */
    w_mood_bg = gfx_make_rect_img(disp, 10, UI_MOOD_Y,
                               220, UI_MOOD_H, GFX_COLOR_DARK_GRAY.full, NULL);

    /* -- mood label (on top of mood bg) -- */
    w_mood = gfx_label_create(disp);
    gfx_obj_set_pos(w_mood, 72, UI_MOOD_Y + 5);
    gfx_label_set_color(w_mood, GFX_COLOR_YELLOW);

    /* -- hint line -- */
    w_hint = gfx_label_create(disp);
    gfx_obj_set_pos(w_hint, 10, UI_HINT_Y);
    gfx_label_set_color(w_hint, GFX_COLOR_GREEN);

    /* -- ctrl status -- */
    w_ctrl = gfx_label_create(disp);
    gfx_obj_set_pos(w_ctrl, 10, UI_CTRL_Y);
    gfx_label_set_color(w_ctrl, GFX_COLOR_GRAY);

    /* -- care hint (above stat bars) -- */
    w_care_hint = gfx_label_create(disp);
    gfx_obj_set_pos(w_care_hint, 10, UI_STATBAR_Y - 12 + 1);
    gfx_label_set_color(w_care_hint, GFX_COLOR_GRAY);

    /* -- rainbow gradient bar -- */
    int rw = 240, rh = UI_GRAD_H;
    uint16_t *rbuf = calloc(1, rw * rh * 2);
    gfx_image_dsc_t *rdsc = calloc(1, sizeof(gfx_image_dsc_t));
    if (rbuf && rdsc) {
        gfx_buf_rainbow_h(rbuf, rw, rh);
        gfx_init_dsc(rdsc, rbuf, rw, rh);
        w_rainbow = gfx_img_create(disp);
        gfx_img_set_src(w_rainbow, (void *)rdsc);
        gfx_obj_set_pos(w_rainbow, 0, UI_GRAD_Y);
    }

    /* -- stat bars (3 × colour-coded rectangle) -- */
    const struct { uint16_t color; const char *label; } bar_cfg[] = {
        { GFX_COLOR_RED.full,   "Hunger" },
        { GFX_COLOR_GREEN.full, "Happy"  },
        { GFX_COLOR_BLUE.full,  "Energy" },
    };
    int bar_x = 58, bar_full_w = 162, bar_h = UI_STATBAR_H;
    for (int i = 0; i < 3; i++) {
        int by = UI_STATBAR_Y + i * 9;
        /* label */
        gfx_obj_t *lbl = gfx_label_create(disp);
        gfx_obj_set_pos(lbl, 10, by - 1);
        gfx_label_set_color(lbl, GFX_COLOR_WHITE);
        gfx_label_set_text(lbl, bar_cfg[i].label);

        /* coloured bar image */
        g_bar[i].x = bar_x;
        g_bar[i].full_w = bar_full_w;
        g_bar[i].buf = calloc(1, bar_full_w * bar_h * 2);
        assert(g_bar[i].buf);
        gfx_make_solid_dsc(&g_bar[i].dsc, g_bar[i].buf,
                       bar_full_w, bar_h, bar_cfg[i].color);
        g_bar[i].obj = gfx_img_create(disp);
        gfx_img_set_src(g_bar[i].obj, (void *)&g_bar[i].dsc);
        gfx_obj_set_pos(g_bar[i].obj, bar_x, by);
    }

    /* -- gamepad debug labels -- */
    w_dbg_title = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_title, 10, 148);
    gfx_label_set_color(w_dbg_title, GFX_COLOR_CYAN);

    w_dbg_ls = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_ls, 10, 156);

    w_dbg_rs = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_rs, 10, 164);

    w_dbg_trig = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_trig, 10, 172);

    w_dbg_btns1 = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_btns1, 10, 180);

    w_dbg_btns2 = gfx_label_create(disp);
    gfx_obj_set_pos(w_dbg_btns2, 10, 188);

    g_dbg_visible = false;
    gfx_obj_set_visible(w_dbg_title, false);
    gfx_obj_set_visible(w_dbg_ls, false);
    gfx_obj_set_visible(w_dbg_rs, false);
    gfx_obj_set_visible(w_dbg_trig, false);
    gfx_obj_set_visible(w_dbg_btns1, false);
    gfx_obj_set_visible(w_dbg_btns2, false);

    /* -- speech bubble -- */
    int bw = UI_BUBBLE_W, bh = UI_BUBBLE_MAX_H;
    g_bubble_buf = calloc(1, bw * bh * 2);
    if (g_bubble_buf) {
        gfx_make_solid_dsc(&g_bubble_dsc, g_bubble_buf,
                           bw, bh, 0x4228); /* dark-ish bg */
    }
    w_bubble_bg = gfx_img_create(disp);
    w_bubble_text = gfx_label_create(disp);
    gfx_label_set_color(w_bubble_text, GFX_COLOR_WHITE);
    gfx_label_set_long_mode(w_bubble_text, GFX_LABEL_LONG_WRAP);
    gfx_obj_set_visible(w_bubble_bg, false);
    gfx_obj_set_visible(w_bubble_text, false);

    ESP_LOGI(TAG, "UI widgets created");

    ui_draw_static();
}

/* ── Stat bars ────────────────────────────────────────────────── */

void ui_update_stat_bars(const pet_stats_t *stats) {
    /* care hint */
    char hint[32];
    snprintf(hint, sizeof(hint), "Action: %s   (cooldown)",
             ui_care_names[ui_care_index]);
    gfx_label_set_text(w_care_hint, hint);

    /* stat values & colors */
    struct { int8_t val; uint16_t color; } bars[] = {
        { stats->hunger,    GFX_COLOR_RED.full },
        { stats->happiness, GFX_COLOR_GREEN.full },
        { stats->energy,    GFX_COLOR_BLUE.full },
    };

    int bar_h = UI_STATBAR_H;
    for (int i = 0; i < 3; i++) {
        int fill_w = (bars[i].val > 0)
            ? (g_bar[i].full_w * bars[i].val / PET_STAT_MAX) : 0;
        if (fill_w < 1 && bars[i].val > 0) fill_w = 1;

        /* fill full buffer with dark gray, then colour the filled portion */
        gfx_buf_fill(g_bar[i].buf, g_bar[i].full_w, bar_h, GFX_COLOR_DARK_GRAY.full);
        if (fill_w > 0) {
            for (int y = 0; y < bar_h; y++) {
                for (int x = 0; x < fill_w; x++) {
                    g_bar[i].buf[y * g_bar[i].full_w + x] = bars[i].color;
                }
            }
        }

        /* re-set src to trigger redraw */
        gfx_img_set_src(g_bar[i].obj, (void *)&g_bar[i].dsc);
    }
}

/* ── Mood label ───────────────────────────────────────────────── */

void ui_update_mood_label(pet_mood_t mood, bool sleeping) {
    const char *label = sleeping ? "Sleeping" : ui_mood_label_text(mood);
    gfx_color_t lc = sleeping ? GFX_COLOR_CYAN :
                     (mood == PET_MOOD_HUNGRY ? GFX_COLOR_RED :
                      mood == PET_MOOD_SAD ? GFX_COLOR_BLUE : GFX_COLOR_YELLOW);
    gfx_label_set_text(w_mood, label);
    gfx_label_set_color(w_mood, lc);
}

/* ── Gamepad debug ────────────────────────────────────────────── */

static const char *btn_names[] = {
    [CTRL_A] = "A", [CTRL_B] = "B", [CTRL_X] = "X", [CTRL_Y] = "Y",
    [CTRL_LB] = "LB", [CTRL_RB] = "RB",
    [CTRL_DPAD_UP] = "DU", [CTRL_DPAD_DOWN] = "DD",
    [CTRL_DPAD_LEFT] = "DL", [CTRL_DPAD_RIGHT] = "DR",
    [CTRL_START] = "St", [CTRL_SELECT] = "Se",
};

void ui_update_gamepad_debug(const gamepad_state_t *gs) {
    char buf[128];

    gfx_label_set_text(w_dbg_title, "--- Gamepad ---");

    snprintf(buf, sizeof(buf), "L X:%+4d Y:%+4d",
             (int)gs->axis_x, (int)gs->axis_y);
    gfx_label_set_text(w_dbg_ls, buf);

    snprintf(buf, sizeof(buf), "R X:%+4d Y:%+4d",
             (int)gs->axis_rx, (int)gs->axis_ry);
    gfx_label_set_text(w_dbg_rs, buf);

    snprintf(buf, sizeof(buf), "LT:%3d  RT:%3d",
             (int)gs->brake, (int)gs->throttle);
    gfx_label_set_text(w_dbg_trig, buf);

    /* button row 1 */
    static const ctrl_button_t row1[] = {
        CTRL_A, CTRL_B, CTRL_X, CTRL_Y, CTRL_LB, CTRL_RB
    };
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row1[i]);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "[%c]%-2s ",
                        on ? '*' : ' ', btn_names[row1[i]]);
    }
    gfx_label_set_text(w_dbg_btns1, buf);

    /* button row 2 */
    static const ctrl_button_t row2[] = {
        CTRL_DPAD_UP, CTRL_DPAD_DOWN, CTRL_DPAD_LEFT,
        CTRL_DPAD_RIGHT, CTRL_START, CTRL_SELECT
    };
    pos = 0;
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row2[i]);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "[%c]%-2s ",
                        on ? '*' : ' ', btn_names[row2[i]]);
    }
    gfx_label_set_text(w_dbg_btns2, buf);

    if (!g_dbg_visible) {
        gfx_obj_set_visible(w_dbg_title, true);
        gfx_obj_set_visible(w_dbg_ls, true);
        gfx_obj_set_visible(w_dbg_rs, true);
        gfx_obj_set_visible(w_dbg_trig, true);
        gfx_obj_set_visible(w_dbg_btns1, true);
        gfx_obj_set_visible(w_dbg_btns2, true);
        g_dbg_visible = true;
    }
}

void ui_update_ctrl_status(bool connected) {
    gfx_label_set_text(w_ctrl,
        connected ? "Ctrl: connected" : "Ctrl: searching...");
    gfx_label_set_color(w_ctrl,
        connected ? GFX_COLOR_GREEN : GFX_COLOR_GRAY);
}

/* ── Speech bubble ────────────────────────────────────────────── */

void ui_speech_render(void) {
    bool vis = ui_speech_visible();
    gfx_obj_set_visible(w_bubble_bg, vis);
    gfx_obj_set_visible(w_bubble_text, vis);

    if (!vis) return;

    const char *text = ui_speech_text();
    gfx_label_set_text(w_bubble_text, text);
    gfx_obj_set_pos(w_bubble_bg, UI_BUBBLE_X, UI_BUBBLE_Y);
    gfx_obj_set_pos(w_bubble_text, UI_BUBBLE_X + 8, UI_BUBBLE_Y + 7);

    /* update bubble bg to trigger redraw */
    if (g_bubble_buf && g_bubble_dsc.data) {
        gfx_img_set_src(w_bubble_bg, (void *)&g_bubble_dsc);
    }
}
