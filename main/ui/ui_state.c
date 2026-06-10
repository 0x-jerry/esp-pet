#include "ui_state.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_state";

/* ── Care action state ────────────────────────────────────── */

int ui_care_index = 0;
const char *ui_care_names[3] = { "Feed", "Play", "Sleep" };

/* ── Mood labels ──────────────────────────────────────────── */

static const char *mood_labels[] = {
    [PET_MOOD_HAPPY]   = "Happy",
    [PET_MOOD_NEUTRAL] = "Neutral",
    [PET_MOOD_SAD]     = "Sad",
    [PET_MOOD_HUNGRY]  = "Hungry",
    [PET_MOOD_SLEEPY]  = "Sleepy",
};

const char *ui_mood_label_text(pet_mood_t mood) {
    if (mood < 0 || mood >= PET_MOOD_COUNT) return "?";
    return mood_labels[mood];
}

/* ── Speech bubble state ──────────────────────────────────── */

#define UI_BUBBLE_TIMEOUT_MS 8000

static char         g_bubble_text[512];
static bool         g_bubble_visible;
static int64_t      g_bubble_shown_ms;
static esp_timer_handle_t g_bubble_timer;

/* bubble auto-dismiss callback */
static void bubble_timeout_cb(void *arg) {
    ui_speech_dismiss();
}

void ui_speech_show(const char *text) {
    strncpy(g_bubble_text, text, sizeof(g_bubble_text) - 1);
    g_bubble_text[sizeof(g_bubble_text) - 1] = '\0';
    g_bubble_visible = true;
    g_bubble_shown_ms = esp_timer_get_time() / 1000;

    /* start/restart dismiss timer */
    if (g_bubble_timer) {
        esp_timer_stop(g_bubble_timer);
        esp_timer_delete(g_bubble_timer);
        g_bubble_timer = NULL;
    }

    const esp_timer_create_args_t args = {
        .callback = bubble_timeout_cb,
        .name = "bubble_timeout",
    };
    esp_timer_create(&args, &g_bubble_timer);
    esp_timer_start_once(g_bubble_timer, UI_BUBBLE_TIMEOUT_MS * 1000);

    ESP_LOGI(TAG, "Speech bubble shown (%d chars)", (int)strlen(text));
}

void ui_speech_dismiss(void) {
    g_bubble_visible = false;
    if (g_bubble_timer) {
        esp_timer_stop(g_bubble_timer);
        esp_timer_delete(g_bubble_timer);
        g_bubble_timer = NULL;
    }
}

bool ui_speech_visible(void) {
    return g_bubble_visible;
}

const char *ui_speech_text(void) {
    return g_bubble_text;
}
