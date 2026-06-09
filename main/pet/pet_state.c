#include "pet_state.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "pet_state";

/* ── Persistent state ───────────────────────────────────────── */

static pet_stats_t   g_stats;
static char          g_name[PET_NAME_MAX + 1];
static char          g_personality[PET_PERSONALITY_MAX + 1];
static bool          g_sleeping;
static pet_mood_t    g_mood;

/* ── Cooldown tracking ──────────────────────────────────────── */

static int64_t g_last_care_ms;   /**< Timestamp of last care action (ms). */
static int64_t g_last_save_ms;   /**< Timestamp of last NVS save (ms). */
static bool    g_dirty;          /**< Set when stats change, cleared on save. */

/* ── Timer handle ───────────────────────────────────────────── */

static esp_timer_handle_t g_tick_timer;

/* ── NVS key constants ──────────────────────────────────────── */

static const char *NVS_NS = "pet";
#define NVS_KEY_HUNGER     "hunger"
#define NVS_KEY_HAPPINESS  "happy"
#define NVS_KEY_ENERGY     "energy"
#define NVS_KEY_AGE        "age"
#define NVS_KEY_NAME       "name"
#define NVS_KEY_PERSONAL   "persona"
#define NVS_KEY_SLEEPING   "sleep"

/* ── Forward declarations ───────────────────────────────────── */

static void load_from_nvs(void);
static void save_to_nvs(void);
static void calc_mood(void);
static void tick_callback(void *arg);

/* ── Helpers ────────────────────────────────────────────────── */

static int8_t clamp_stat(int v) {
    if (v < PET_STAT_MIN) return PET_STAT_MIN;
    if (v > PET_STAT_MAX) return PET_STAT_MAX;
    return (int8_t)v;
}

static void mark_dirty(void) {
    g_dirty = true;
}

/* ── NVS persistence ────────────────────────────────────────── */

static void load_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No pet NVS data found, using defaults");
        return;
    }

    int8_t val8;
    uint32_t val32;
    size_t len;

    err = nvs_get_i8(handle, NVS_KEY_HUNGER, &val8);
    g_stats.hunger = (err == ESP_OK) ? val8 : 80;

    err = nvs_get_i8(handle, NVS_KEY_HAPPINESS, &val8);
    g_stats.happiness = (err == ESP_OK) ? val8 : 80;

    err = nvs_get_i8(handle, NVS_KEY_ENERGY, &val8);
    g_stats.energy = (err == ESP_OK) ? val8 : 80;

    err = nvs_get_u32(handle, NVS_KEY_AGE, &val32);
    g_stats.age_ticks = (err == ESP_OK) ? val32 : 0;

    len = sizeof(g_name);
    err = nvs_get_str(handle, NVS_KEY_NAME, g_name, &len);
    if (err != ESP_OK) {
        strncpy(g_name, "Pippy", PET_NAME_MAX);
    }

    len = sizeof(g_personality);
    err = nvs_get_str(handle, NVS_KEY_PERSONAL, g_personality, &len);
    if (err != ESP_OK) {
        strncpy(g_personality,
                "A cute small fox-like creature. Playful, sometimes mischievous.",
                PET_PERSONALITY_MAX);
    }

    err = nvs_get_i8(handle, NVS_KEY_SLEEPING, &val8);
    g_sleeping = (err == ESP_OK) ? (val8 != 0) : false;

    nvs_close(handle);

    ESP_LOGI(TAG, "Loaded pet from NVS: hunger=%d happy=%d energy=%d age=%lu sleep=%d",
             g_stats.hunger, g_stats.happiness, g_stats.energy,
             (unsigned long)g_stats.age_ticks, g_sleeping);
}

static void save_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %d", err);
        return;
    }

    nvs_set_i8(handle, NVS_KEY_HUNGER, g_stats.hunger);
    nvs_set_i8(handle, NVS_KEY_HAPPINESS, g_stats.happiness);
    nvs_set_i8(handle, NVS_KEY_ENERGY, g_stats.energy);
    nvs_set_u32(handle, NVS_KEY_AGE, g_stats.age_ticks);
    nvs_set_str(handle, NVS_KEY_NAME, g_name);
    nvs_set_str(handle, NVS_KEY_PERSONAL, g_personality);
    nvs_set_i8(handle, NVS_KEY_SLEEPING, g_sleeping ? 1 : 0);

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", err);
    }
    nvs_close(handle);

    g_dirty = false;
    g_last_save_ms = esp_timer_get_time() / 1000;

    ESP_LOGD(TAG, "Saved pet to NVS");
}

void pet_save(void) {
    if (g_dirty) {
        save_to_nvs();
    }
}

/* ── Mood calculation ───────────────────────────────────────── */

static void calc_mood(void) {
    if (g_sleeping) {
        g_mood = PET_MOOD_SLEEPY;
        return;
    }
    if (g_stats.hunger < 25) {
        g_mood = PET_MOOD_HUNGRY;
    } else if (g_stats.happiness < 25) {
        g_mood = PET_MOOD_SAD;
    } else if (g_stats.energy < 20) {
        g_mood = PET_MOOD_SLEEPY;
    } else if (g_stats.happiness > 70 && g_stats.hunger > 50) {
        g_mood = PET_MOOD_HAPPY;
    } else {
        g_mood = PET_MOOD_NEUTRAL;
    }
}

pet_mood_t pet_get_mood(void) {
    return g_mood;
}

/* ── Stat decay (timer callback) ────────────────────────────── */

static void tick_callback(void *arg) {
    g_stats.age_ticks++;

    if (g_sleeping) {
        /* Sleeping: energy recovers, hunger decays faster, happiness slow decay */
        g_stats.energy    = clamp_stat(g_stats.energy + 3);
        g_stats.hunger    = clamp_stat(g_stats.hunger - 2);
        g_stats.happiness = clamp_stat(g_stats.happiness - 1);
    } else {
        g_stats.hunger    = clamp_stat(g_stats.hunger - 1);
        g_stats.happiness = clamp_stat(g_stats.happiness - 1);
        g_stats.energy    = clamp_stat(g_stats.energy - 1);
    }

    calc_mood();
    mark_dirty();

    /* Auto-save every 60 seconds if dirty */
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (g_dirty && (now_ms - g_last_save_ms > 60000)) {
        save_to_nvs();
    }

    ESP_LOGD(TAG, "Tick: hunger=%d happy=%d energy=%d mood=%d sleep=%d",
             g_stats.hunger, g_stats.happiness, g_stats.energy,
             g_mood, g_sleeping);
}

void pet_tick(void) {
    tick_callback(NULL);
}

/* ── Care actions ───────────────────────────────────────────── */

static bool care_cooldown_ok(void) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    return (now_ms - g_last_care_ms >= PET_CARE_COOLDOWN_MS);
}

bool pet_feed(void) {
    if (!care_cooldown_ok()) {
        ESP_LOGD(TAG, "Feed rejected: cooldown active");
        return false;
    }
    g_stats.hunger    = clamp_stat(g_stats.hunger + PET_FEED_HUNGER_BOOST);
    g_stats.happiness = clamp_stat(g_stats.happiness + PET_FEED_HAPPINESS_BOOST);
    g_last_care_ms    = esp_timer_get_time() / 1000;
    calc_mood();
    mark_dirty();
    ESP_LOGI(TAG, "Pet fed: hunger=%d happy=%d", g_stats.hunger, g_stats.happiness);
    return true;
}

bool pet_play(void) {
    if (!care_cooldown_ok()) {
        ESP_LOGD(TAG, "Play rejected: cooldown active");
        return false;
    }
    g_stats.happiness = clamp_stat(g_stats.happiness + PET_PLAY_HAPPINESS_BOOST);
    g_stats.energy    = clamp_stat(g_stats.energy - PET_PLAY_ENERGY_COST);
    g_last_care_ms    = esp_timer_get_time() / 1000;
    calc_mood();
    mark_dirty();
    ESP_LOGI(TAG, "Pet played: happy=%d energy=%d", g_stats.happiness, g_stats.energy);
    return true;
}

bool pet_sleep(void) {
    if (!care_cooldown_ok()) {
        ESP_LOGD(TAG, "Sleep toggle rejected: cooldown active");
        return false;
    }
    g_sleeping = !g_sleeping;
    g_last_care_ms = esp_timer_get_time() / 1000;
    calc_mood();
    mark_dirty();
    ESP_LOGI(TAG, "Pet sleep toggled: now %s", g_sleeping ? "sleeping" : "awake");
    return true;
}

/* ── Queries ────────────────────────────────────────────────── */

const pet_stats_t *pet_get_stats(void) {
    return &g_stats;
}

const char *pet_get_name(void) {
    return g_name;
}

const char *pet_get_personality(void) {
    return g_personality;
}

bool pet_is_sleeping(void) {
    return g_sleeping;
}

/* ── Configuration ──────────────────────────────────────────── */

void pet_set_name(const char *name) {
    strncpy(g_name, name, PET_NAME_MAX);
    g_name[PET_NAME_MAX] = '\0';
    mark_dirty();
    save_to_nvs();
}

void pet_set_personality(const char *personality) {
    strncpy(g_personality, personality, PET_PERSONALITY_MAX);
    g_personality[PET_PERSONALITY_MAX] = '\0';
    mark_dirty();
    save_to_nvs();
}

/* ── AI context builder ─────────────────────────────────────── */

static const char *mood_str(pet_mood_t m) {
    switch (m) {
    case PET_MOOD_HAPPY:  return "happy";
    case PET_MOOD_NEUTRAL: return "neutral";
    case PET_MOOD_SAD:    return "sad";
    case PET_MOOD_HUNGRY: return "hungry";
    case PET_MOOD_SLEEPY: return "sleepy";
    default:              return "unknown";
    }
}

int pet_build_context_str(char *buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "Pet Name: %s\n"
        "Personality: %s\n"
        "Stats: hunger=%d/100, happiness=%d/100, energy=%d/100\n"
        "Mood: %s\n"
        "Status: %s",
        g_name,
        g_personality,
        g_stats.hunger, g_stats.happiness, g_stats.energy,
        mood_str(g_mood),
        g_sleeping ? "sleeping" : "awake"
    );
}

/* ── Lifecycle ──────────────────────────────────────────────── */

void pet_init(void) {
    ESP_LOGI(TAG, "Initializing pet state manager...");

    /* Default values (overwritten by NVS load if present) */
    g_stats.hunger    = 80;
    g_stats.happiness = 80;
    g_stats.energy    = 80;
    g_stats.age_ticks = 0;
    g_sleeping        = false;
    g_dirty           = false;
    g_last_care_ms    = 0;
    g_last_save_ms    = 0;

    strncpy(g_name, "Pippy", PET_NAME_MAX);
    strncpy(g_personality,
            "A cute small fox-like creature. Playful, sometimes mischievous.",
            PET_PERSONALITY_MAX);

    /* Try to restore from NVS */
    load_from_nvs();
    calc_mood();

    /* Start periodic tick timer */
    const esp_timer_create_args_t timer_args = {
        .callback = tick_callback,
        .arg      = NULL,
        .name     = "pet_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_tick_timer,
        (uint64_t)PET_TICK_INTERVAL_S * 1000000));

    /* Initial save */
    save_to_nvs();

    ESP_LOGI(TAG, "Pet ready: hunger=%d happy=%d energy=%d mood=%d sleep=%d",
             g_stats.hunger, g_stats.happiness, g_stats.energy,
             g_mood, g_sleeping);
}
