#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pet/pet_state.h"
#include "gamepad/controller.h"

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

/** Speech bubble dimensions. */
#define UI_BUBBLE_X  10
#define UI_BUBBLE_Y  60
#define UI_BUBBLE_W  220
#define UI_BUBBLE_MAX_H 140
#define UI_BUBBLE_TIMEOUT_MS 8000

/* ── Care action index (shared with main) ─────────────────── */
extern int ui_care_index;
extern const char *ui_care_names[3];

/* ── Static UI ────────────────────────────────────────────── */

/** Draw all static elements (title, mood bar, hints, gradient). */
void ui_draw_static(void);

/* ── Dynamic UI ───────────────────────────────────────────── */

/** Redraw stat bars with current values + care hint. */
void ui_draw_stat_bars(const pet_stats_t *stats);

/** Update mood label text and color. */
void ui_draw_mood_label(pet_mood_t mood, bool sleeping);

/** Draw gamepad debug overlay. */
void ui_draw_gamepad_debug(const gamepad_state_t *gs);

/** Draw controller connection status line. */
void ui_draw_ctrl_status(bool connected);

/** Clear the gamepad debug area (on disconnect). */
void ui_clear_gamepad_debug(void);

/* ── Speech bubble ────────────────────────────────────────── */

/**
 * Show a speech bubble with the given text.
 * Auto-dismisses after UI_BUBBLE_TIMEOUT_MS.
 * Calling again replaces the current bubble.
 */
void ui_speech_show(const char *text);

/**
 * Dismiss the speech bubble immediately.
 */
void ui_speech_dismiss(void);

/**
 * Check if speech bubble is currently visible.
 */
bool ui_speech_visible(void);

/**
 * Render the speech bubble overlay (called each frame).
 * No-op if no bubble is visible.
 */
void ui_speech_render(void);
