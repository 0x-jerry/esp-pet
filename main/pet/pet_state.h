#pragma once

#include <stdint.h>
#include <stdbool.h>

/** Maximum length for pet name and personality strings (NVS-friendly). */
#define PET_NAME_MAX        24
#define PET_PERSONALITY_MAX 128
#define PET_CONTEXT_MAX     512

/** Stat ranges. */
#define PET_STAT_MIN 0
#define PET_STAT_MAX 100

/** Cooldown between care actions (milliseconds). */
#define PET_CARE_COOLDOWN_MS 3000

/** Stat decay interval (seconds) — how often stats tick down. */
#define PET_TICK_INTERVAL_S 10

/** Stat change amounts per action. */
#define PET_FEED_HUNGER_BOOST    25
#define PET_FEED_HAPPINESS_BOOST 5
#define PET_PLAY_HAPPINESS_BOOST 25
#define PET_PLAY_ENERGY_COST     10
#define PET_SLEEP_ENERGY_BOOST   30

/**
 * Mood derived from stat thresholds.
 */
typedef enum {
    PET_MOOD_HAPPY,
    PET_MOOD_NEUTRAL,
    PET_MOOD_SAD,
    PET_MOOD_HUNGRY,
    PET_MOOD_SLEEPY,
    PET_MOOD_COUNT
} pet_mood_t;

/**
 * Core pet statistics.
 * All values clamped to [PET_STAT_MIN, PET_STAT_MAX].
 */
typedef struct {
    int8_t hunger;      /**< 0=starving, 100=full */
    int8_t happiness;   /**< 0=miserable, 100=ecstatic */
    int8_t energy;      /**< 0=exhausted, 100=energetic */
    uint32_t age_ticks; /**< Total number of tick intervals elapsed */
} pet_stats_t;

/**
 * Full pet context sent to the AI for in-character responses.
 */
typedef struct {
    char name[PET_NAME_MAX + 1];
    char personality[PET_PERSONALITY_MAX + 1];
    pet_stats_t stats;
    pet_mood_t mood;
} pet_context_t;

/* ── Lifecycle ──────────────────────────────────────────────── */

/**
 * Initialize pet state: restore from NVS (or create defaults),
 * start the periodic stat-decay timer.
 */
void pet_init(void);

/**
 * Force save current state to NVS immediately.
 * Normally saving is automatic on changes, but call before deep sleep/shutdown.
 */
void pet_save(void);

/* ── Stat decay ─────────────────────────────────────────────── */

/**
 * Manually trigger one tick of stat decay (hunger -1, happiness -1,
 * energy -1 when not sleeping). Called automatically by the internal timer;
 * exposed for testing / forced-advance scenarios.
 */
void pet_tick(void);

/* ── Care actions ───────────────────────────────────────────── */

/**
 * Feed the pet: +hunger, +small happiness.
 * @return true if action was accepted (cooldown not active).
 */
bool pet_feed(void);

/**
 * Play with the pet: +happiness, -energy.
 * @return true if action was accepted.
 */
bool pet_play(void);

/**
 * Put pet to sleep / wake: toggles sleep state.
 * While sleeping, energy recovers but hunger decays faster.
 * @return true if accepted.
 */
bool pet_sleep(void);

/* ── Queries ────────────────────────────────────────────────── */

/**
 * Get current mood based on stat thresholds.
 */
pet_mood_t pet_get_mood(void);

/**
 * Get a read-only pointer to the current stats.
 */
const pet_stats_t *pet_get_stats(void);

/**
 * Get the current pet name.
 */
const char *pet_get_name(void);

/**
 * Get the current personality description.
 */
const char *pet_get_personality(void);

/** Return true if currently sleeping. */
bool pet_is_sleeping(void);

/* ── Configuration ──────────────────────────────────────────── */

/**
 * Set pet name (persisted to NVS).
 */
void pet_set_name(const char *name);

/**
 * Set personality description (persisted to NVS).
 */
void pet_set_personality(const char *personality);

/* ── AI context ─────────────────────────────────────────────── */

/**
 * Build a full context snapshot for sending to the AI.
 * Writes into caller-provided buffer of at least PET_CONTEXT_MAX bytes.
 * Returns number of bytes written (excluding null terminator).
 */
int pet_build_context_str(char *buf, size_t buf_size);
