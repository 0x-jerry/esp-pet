#include "ui.h"
#include "pet/pet_sprite.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>

/* ── Master strip callback ────────────────────────────────── */

void ui_render_strip(int16_t y0, int16_t y1) {
    (void)y1; /* y1 not needed — graphics primitives clip to strip via context */

    /* ── Title + underline ── */
    char title[48];
    snprintf(title, sizeof(title), "ESP-PET  — %s", pet_get_name());
    graphics_draw_text(10, UI_TITLE_Y, title, COLOR_WHITE, COLOR_BLACK, 0);
    graphics_fill_rect(10, UI_UNDERLINE_Y, 220, 2, COLOR_WHITE);

    /* ── Mood bar background ── */
    graphics_fill_rect(10, UI_MOOD_Y, 220, UI_MOOD_H, COLOR_DARK_GRAY);
    graphics_draw_text(16, UI_MOOD_Y + 5, "Mood:", COLOR_WHITE, COLOR_DARK_GRAY, 0);

    /* ── Hints ── */
    graphics_draw_text(10, UI_HINT_Y,
        "Btn/A:action  B:talk  D-Pad L/R:cycle", COLOR_GREEN, COLOR_BLACK, 0);

    /* ── Rainbow bar ── */
    graphics_draw_rainbow_h(0, UI_GRAD_Y, DISPLAY_WIDTH, UI_GRAD_H);

    /* ── Pet sprite ── */
    pet_mood_t mood = pet_get_mood();
    bool sleeping = pet_is_sleeping();
    pet_sprite_draw(UI_PET_X, UI_PET_Y, sleeping ? PET_MOOD_SLEEPY : mood, sleeping);

    /* ── Stat bars + care hint ── */
    ui_draw_stat_bars(pet_get_stats());

    /* ── Mood label ── */
    ui_draw_mood_label(mood, sleeping);

    /* ── Controller status ── */
    bool ctrl_conn = controller_is_connected();
    graphics_fill_rect(10, UI_CTRL_Y, 220, 10, COLOR_BLACK);
    graphics_draw_text(10, UI_CTRL_Y,
        ctrl_conn ? "Ctrl: connected" : "Ctrl: searching...",
        ctrl_conn ? COLOR_GREEN : COLOR_GRAY, COLOR_BLACK, 0);

    /* ── Gamepad debug ── */
    if (ctrl_conn) {
        gamepad_state_t gs = controller_get_state();
        ui_draw_gamepad_debug(&gs);
    }

    /* ── Speech bubble overlay ── */
    ui_speech_render();
}

/* ── Stat bars ────────────────────────────────────────────── */

void ui_draw_stat_bars(const pet_stats_t *stats) {
    int16_t y = UI_STATBAR_Y;

    /* care hint */
    graphics_fill_rect(10, y - 12, 220, 12, COLOR_BLACK);
    char hint[32];
    snprintf(hint, sizeof(hint), "Action: %s   (cooldown)", ui_care_names[ui_care_index]);
    graphics_draw_text(10, y - 11, hint, COLOR_GRAY, COLOR_BLACK, 0);

    /* helper: single bar */
    struct { const char *label; int8_t val; uint16_t color; } bars[] = {
        {"Hunger", stats->hunger,    COLOR_RED},
        {"Happy",  stats->happiness, COLOR_GREEN},
        {"Energy", stats->energy,    COLOR_BLUE},
    };

    for (int i = 0; i < 3; i++) {
        int16_t by = y + i * 9;
        graphics_draw_text(10, by - 1, bars[i].label, COLOR_WHITE, COLOR_BLACK, 0);

        int16_t bx = 58, bw = 162;
        graphics_fill_rect(bx, by, bw, UI_STATBAR_H, COLOR_DARK_GRAY);

        if (bars[i].val > 0) {
            int fill_w = (int)bw * bars[i].val / PET_STAT_MAX;
            if (fill_w < 1) fill_w = 1;
            graphics_fill_rect(bx, by, fill_w, UI_STATBAR_H, bars[i].color);
        }
    }
}

/* ── Mood label ───────────────────────────────────────────── */

void ui_draw_mood_label(pet_mood_t mood, bool sleeping) {
    graphics_fill_rect(65, UI_MOOD_Y, 140, UI_MOOD_H, COLOR_DARK_GRAY);

    const char *label = sleeping ? "Sleeping" : ui_mood_label_text(mood);
    uint16_t lc = sleeping ? COLOR_CYAN :
                  (mood == PET_MOOD_HUNGRY ? COLOR_RED :
                   mood == PET_MOOD_SAD ? COLOR_BLUE : COLOR_YELLOW);

    graphics_draw_text(72, UI_MOOD_Y + 5, label, lc, COLOR_DARK_GRAY, 0);
}

/* ── Gamepad debug ────────────────────────────────────────── */

#define DBG_Y UI_DEBUG_Y
#define DBG_W 220

static const char *btn_names[] = {
    [CTRL_A] = "A", [CTRL_B] = "B", [CTRL_X] = "X", [CTRL_Y] = "Y",
    [CTRL_LB] = "LB", [CTRL_RB] = "RB",
    [CTRL_DPAD_UP] = "DU", [CTRL_DPAD_DOWN] = "DD",
    [CTRL_DPAD_LEFT] = "DL", [CTRL_DPAD_RIGHT] = "DR",
    [CTRL_START] = "St", [CTRL_SELECT] = "Se",
};

void ui_draw_gamepad_debug(const gamepad_state_t *gs) {
    char buf[64];
    int y = DBG_Y;

    graphics_fill_rect(10, y, DBG_W, UI_DEBUG_H, COLOR_BLACK);
    graphics_draw_text(10, y, "--- Gamepad ---", COLOR_CYAN, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "L X:%+4d Y:%+4d", (int)gs->axis_x, (int)gs->axis_y);
    graphics_draw_text(10, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "R X:%+4d Y:%+4d", (int)gs->axis_rx, (int)gs->axis_ry);
    graphics_draw_text(10, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "LT:%3d  RT:%3d", (int)gs->brake, (int)gs->throttle);
    graphics_draw_text(10, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    static const ctrl_button_t row1[] = {CTRL_A, CTRL_B, CTRL_X, CTRL_Y, CTRL_LB, CTRL_RB};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row1[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row1[i]]);
        graphics_draw_text(10 + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
    y += 8;

    static const ctrl_button_t row2[] = {CTRL_DPAD_UP, CTRL_DPAD_DOWN, CTRL_DPAD_LEFT,
                                         CTRL_DPAD_RIGHT, CTRL_START, CTRL_SELECT};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row2[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row2[i]]);
        graphics_draw_text(10 + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
}

void ui_clear_gamepad_debug(void) {
    graphics_fill_rect(10, DBG_Y, DBG_W, UI_DEBUG_H, COLOR_BLACK);
}

/* ── Speech bubble rendering ──────────────────────────────── */

void ui_speech_render(void) {
    if (!ui_speech_visible()) return;

    int16_t bx = UI_BUBBLE_X;
    int16_t by = UI_BUBBLE_Y;
    int16_t bw = UI_BUBBLE_W;
    int16_t text_w = bw - 16; /* 8px padding each side */

    /*
     * Calculate required height by simulating text wrap.
     */
    int chars_per_line = text_w / 6; /* FONT_WIDTH=6 */
    const char *text = ui_speech_text();
    int len = strlen(text);
    int lines = 1;
    int line_chars = 0;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            lines++;
            line_chars = 0;
        } else {
            line_chars++;
            if (line_chars >= chars_per_line) { lines++; line_chars = 0; }
        }
    }
    int16_t bh = lines * 8 + 14; /* 7px padding top/bottom */
    if (bh > UI_BUBBLE_MAX_H) bh = UI_BUBBLE_MAX_H;

    /* draw background + border */
    graphics_draw_rounded_rect(bx, by, bw, bh, COLOR(40, 40, 50), COLOR_WHITE);

    /* draw text */
    graphics_draw_text(bx + 8, by + 7, text, COLOR_WHITE,
                       COLOR(40, 40, 50), text_w);
}
