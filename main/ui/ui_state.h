#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pet/pet_state.h"

/* ── Care action state ────────────────────────────────── */

/** Index of the currently selected care action (Feed/Play/Sleep). */
extern int ui_care_index;

/** Human-readable names for each care action. */
extern const char *ui_care_names[3];

/* ── Mood labels ──────────────────────────────────────── */

/**
 * Get the human-readable label for a pet mood.
 * @param mood  The pet_mood_t value.
 * @return String label, or "?" for invalid mood.
 */
const char *ui_mood_label_text(pet_mood_t mood);

/* ── Speech bubble state management ───────────────────── */

/**
 * Show a speech bubble with the given text.
 * Auto-dismisses after UI_BUBBLE_TIMEOUT_MS.
 * Calling again replaces the current bubble.
 */
void ui_speech_show(const char *text);

/** Dismiss the speech bubble immediately. */
void ui_speech_dismiss(void);

/** Check if speech bubble is currently visible. */
bool ui_speech_visible(void);

/**
 * Get the current speech bubble text.
 * @return Pointer to internal buffer (empty string if not visible).
 */
const char *ui_speech_text(void);
