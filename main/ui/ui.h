#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gfx.h"
#include "pet/pet_state.h"
#include "gamepad/controller.h"
#include "ui_state.h"

/* ── Layout constants ─────────────────────────────────────── */
#define UI_TITLE_Y       8
#define UI_UNDERLINE_Y   20
#define UI_MOOD_Y        30
#define UI_MOOD_H        18
#define UI_HINT_Y        48
#define UI_CTRL_Y        196
#define UI_STATBAR_Y     209
#define UI_STATBAR_H     8
#define UI_GRAD_Y        236
#define UI_GRAD_H        4

/** Pet sprite center position. */
#define UI_PET_X  96
#define UI_PET_Y  72

/** Speech bubble rendering dimensions. */
#define UI_BUBBLE_X  10
#define UI_BUBBLE_Y  60
#define UI_BUBBLE_W  220
#define UI_BUBBLE_MAX_H 140

/* ── Lifecycle ───────────────────────────────────────────── */

/**
 * Initialize all UI widgets on a gfx display.
 * Creates labels, image-backed bars, and bubble widgets.
 * Must be called once after gfx_disp_add().
 */
void ui_init(gfx_disp_t *disp);

/* ── Static UI ────────────────────────────────────────────── */

/** Populate static text/elements (title, hints, rainbow bar). */
void ui_draw_static(void);

/* ── Dynamic UI updates ──────────────────────────────────── */

/** Update stat bar widths from current values. */
void ui_update_stat_bars(const pet_stats_t *stats);

/** Update mood label text and color. */
void ui_update_mood_label(pet_mood_t mood, bool sleeping);

/** Update gamepad debug overlay with live state. */
void ui_update_gamepad_debug(const gamepad_state_t *gs);

/** Update controller connection status line. */
void ui_update_ctrl_status(bool connected);

/** Render (show/hide) the speech bubble overlay. */
void ui_speech_render(void);
