#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pet/pet_state.h"
#include "gamepad/controller.h"
#include "ui_state.h"

/* ── Layout constants ─────────────────────────────────────── */
#define UI_TITLE_Y       8
#define UI_UNDERLINE_Y   20
#define UI_MOOD_Y        30
#define UI_MOOD_H        18
#define UI_HINT_Y        48
#define UI_DEBUG_Y       148
#define UI_DEBUG_H       46
#define UI_CTRL_Y        196
#define UI_STATBAR_Y     209
#define UI_STATBAR_H     8
#define UI_GRAD_Y        236
#define UI_GRAD_H        4

/** Pet sprite center position. */
#define UI_PET_X  96    /* left edge: 96 = center of 48-wide sprite at 120 */
#define UI_PET_Y  72    /* top edge */

/** Speech bubble rendering dimensions. */
#define UI_BUBBLE_X  10
#define UI_BUBBLE_Y  60
#define UI_BUBBLE_W  220
#define UI_BUBBLE_MAX_H 140

/* ── Static element renderers ─────────────────────────────── */

/** Draw title bar: "ESP-PET  — <name>" + white underline. */
void ui_draw_title(void);

/** Draw mood bar background (dark gray rectangle + "Mood:" label). */
void ui_draw_mood_bg(void);

/** Draw hint text (button help line). */
void ui_draw_hints(void);

/** Draw the rainbow gradient bar at the bottom. */
void ui_draw_rainbow_bar(void);

/** Draw controller connection status line. */
void ui_draw_ctrl_status(bool connected);

/* ── Dynamic element renderers ────────────────────────────── */

/** Redraw stat bars with current values + care hint. */
void ui_draw_stat_bars(const pet_stats_t *stats);

/** Update mood label text and color. */
void ui_draw_mood_label(pet_mood_t mood, bool sleeping);

/** Draw gamepad debug overlay. */
void ui_draw_gamepad_debug(const gamepad_state_t *gs);

/** Clear the gamepad debug area (on disconnect). */
void ui_clear_gamepad_debug(void);

/**
 * Render the speech bubble overlay (called each frame).
 * No-op if no bubble is visible.
 */
void ui_speech_render(void);
