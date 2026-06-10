#include "ui.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>

/* ── Static UI ────────────────────────────────────────────── */

void ui_draw_static(void) {

    /* title + name */
    char title[48];
    snprintf(title, sizeof(title), "ESP-PET  — %s", pet_get_name());
    graphics_draw_text(10, UI_TITLE_Y, title, COLOR_WHITE, COLOR_BLACK, 0);
    graphics_fill_rect(10, UI_UNDERLINE_Y, 220, 2, COLOR_WHITE);

    /* mood bar */
    graphics_fill_rect(10, UI_MOOD_Y, 220, UI_MOOD_H, COLOR_DARK_GRAY);
    graphics_draw_text(16, UI_MOOD_Y + 5, "Mood:", COLOR_WHITE, COLOR_DARK_GRAY, 0);

    /* hints */
    graphics_draw_text(10, UI_HINT_Y,
        "Btn/A:action  B:talk  D-Pad L/R:cycle", COLOR_GREEN, COLOR_BLACK, 0);

    /* controller status placeholder */
    graphics_draw_text(10, UI_CTRL_Y, "Ctrl: searching...", COLOR_GRAY, COLOR_BLACK, 0);

    /* initial stat bars */
    ui_draw_stat_bars(pet_get_stats());

    /* rainbow bar */
    graphics_draw_rainbow_h(0, UI_GRAD_Y, DISPLAY_WIDTH, UI_GRAD_H);
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

#define DBG_X UI_DEBUG_Y
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
    int y = DBG_X;

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
    graphics_fill_rect(10, DBG_X, DBG_W, UI_DEBUG_H, COLOR_BLACK);
}

void ui_draw_ctrl_status(bool connected) {
    graphics_fill_rect(10, UI_CTRL_Y, 220, 10, COLOR_BLACK);
    graphics_draw_text(10, UI_CTRL_Y,
        connected ? "Ctrl: connected" : "Ctrl: searching...",
        connected ? COLOR_GREEN : COLOR_GRAY, COLOR_BLACK, 0);
}

/* ── Speech bubble rendering ──────────────────────────────── */

static void draw_bubble_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h,
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
            /* bottom-left */
            graphics_fill_rect(x + r - dx_max, y + h - r + dy - 1, dx_max, 1, bg);
        }
    }
    /* simple border via thin lines */
    graphics_fill_rect(x, y + r, 1, h - 2 * r, border);
    graphics_fill_rect(x + w - 1, y + r, 1, h - 2 * r, border);
    graphics_fill_rect(x + r, y, w - 2 * r, 1, border);
    graphics_fill_rect(x + r, y + h - 1, w - 2 * r, 1, border);
}

/**
 * Render the speech bubble overlay. Called each frame by display task.
 * Redraws the bubble and wrapped text.
 */
void ui_speech_render(void) {
    if (!ui_speech_visible()) return;

    int16_t bx = UI_BUBBLE_X;
    int16_t by = UI_BUBBLE_Y;
    int16_t bw = UI_BUBBLE_W;
    int16_t text_w = bw - 16; /* 8px padding each side */

    /*
     * Calculate required height by simulating text wrap.
     * We call graphics_draw_text with the actual string later,
     * but here we roughly estimate: max_chars_per_line ≈ text_w / FONT_WIDTH,
     * and total lines ≈ len / chars_per_line.
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
    draw_bubble_rounded_rect(bx, by, bw, bh, COLOR(40, 40, 50), COLOR_WHITE);

    /* draw text */
    graphics_draw_text(bx + 8, by + 7, text, COLOR_WHITE,
                       COLOR(40, 40, 50), text_w);
}
