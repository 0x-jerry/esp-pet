#include "buttons.h"
#include "gamepad/controller.h"
#include "display.h"
#include "graphics.h"
#include "pet/pet_state.h"
#include "gamepad/xbox_ble.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── NimBLE native headers ────────────────────────────────────── */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *TAG = "main";

/* ── Mood display labels ─────────────────────────────────── */
static const char *mood_labels[] = {
    [PET_MOOD_HAPPY]  = "Happy",
    [PET_MOOD_NEUTRAL] = "Neutral",
    [PET_MOOD_SAD]    = "Sad",
    [PET_MOOD_HUNGRY] = "Hungry",
    [PET_MOOD_SLEEPY] = "Sleepy",
};

/* ── Care action names (cycled by Action button) ─────────── */
static const char *care_names[] = { "Feed", "Play", "Sleep" };
static int current_care = 0;

/* ── Layout zones (mutually exclusive, no overlap) ────────── */
#define LAYOUT_TITLE_Y       8
#define LAYOUT_UNDERLINE_Y   20
#define LAYOUT_MOOD_Y        30
#define LAYOUT_MOOD_H        18
#define LAYOUT_DEBUG_Y       148
#define LAYOUT_DEBUG_H       46
#define LAYOUT_CTRL_Y        196
#define LAYOUT_STATBAR_Y     209
#define LAYOUT_STATBAR_H     8
#define LAYOUT_GRAD_Y        236
#define LAYOUT_GRAD_H        4
#define LAYOUT_HINT_Y        48

/* ── Face (pet sprite area) ──────────────────────────────── */
#define FACE_CX  120
#define FACE_CY  120
#define FACE_R   30
#define EYE_Y    (FACE_CY - 9)
#define EYE_W    5
#define EYE_H    4
#define EYE_LX   108
#define EYE_RX   (2 * FACE_CX - EYE_LX - EYE_W + 1)

/* ── Face drawing helpers ────────────────────────────────── */

static void draw_face_circle(uint16_t color) {
    int16_t r = FACE_R, cx = FACE_CX, cy = FACE_CY;
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                graphics_draw_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

static void draw_face_eyes(pet_mood_t mood) {
    switch (mood) {
    case PET_MOOD_SLEEPY:
        /* closed eyes: horizontal lines */
        graphics_fill_rect(EYE_LX, EYE_Y + EYE_H/2, EYE_W, 1, COLOR_BLACK);
        graphics_fill_rect(EYE_RX, EYE_Y + EYE_H/2, EYE_W, 1, COLOR_BLACK);
        break;
    case PET_MOOD_HUNGRY:
        /* wide eyes */
        graphics_fill_rect(EYE_LX, EYE_Y - 1, EYE_W, EYE_H + 2, COLOR_BLACK);
        graphics_fill_rect(EYE_RX, EYE_Y - 1, EYE_W, EYE_H + 2, COLOR_BLACK);
        break;
    default:
        graphics_fill_rect(EYE_LX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        graphics_fill_rect(EYE_RX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        break;
    }
}

static void draw_face_mouth(pet_mood_t mood) {
    switch (mood) {
    case PET_MOOD_HAPPY: {
        int16_t my = FACE_CY + 6;
        for (int16_t dx = -8; dx <= 8; dx++) {
            int16_t dy = -(dx * dx) / 10 + 4;
            graphics_draw_pixel(FACE_CX + dx, my + dy, COLOR_BLACK);
            graphics_draw_pixel(FACE_CX + dx, my + dy + 1, COLOR_BLACK);
        }
        break;
    }
    case PET_MOOD_SAD: {
        int16_t my = FACE_CY + 16;
        for (int16_t dx = -8; dx <= 8; dx++) {
            int16_t dy = dx * dx / 10 - 4;
            graphics_draw_pixel(FACE_CX + dx, my + dy, COLOR_BLACK);
            graphics_draw_pixel(FACE_CX + dx, my + dy + 1, COLOR_BLACK);
        }
        break;
    }
    case PET_MOOD_HUNGRY:
        /* open mouth (oval) */
        for (int16_t dy = -5; dy <= 5; dy++) {
            for (int16_t dx = -4; dx <= 4; dx++) {
                if (dx * dx * 2 + dy * dy <= 30) {
                    graphics_draw_pixel(FACE_CX + dx, FACE_CY + 9 + dy, COLOR_BLACK);
                }
            }
        }
        break;
    case PET_MOOD_SLEEPY:
        /* tiny 'o' mouth */
        for (int16_t dy = -2; dy <= 2; dy++) {
            for (int16_t dx = -2; dx <= 2; dx++) {
                if (dx * dx + dy * dy <= 5) {
                    graphics_draw_pixel(FACE_CX + dx, FACE_CY + 10 + dy, COLOR_BLACK);
                }
            }
        }
        break;
    default: /* neutral */
        graphics_fill_rect(FACE_CX - 5, FACE_CY + 9, 10, 2, COLOR_BLACK);
        break;
    }
}

static void draw_pet_face(pet_mood_t mood) {
    int16_t sz = (FACE_R + 8) * 2;
    graphics_fill_rect(FACE_CX - (FACE_R + 8), FACE_CY - (FACE_R + 8),
                      sz, sz, COLOR_BLACK);

    /* pick face color by mood */
    uint16_t face_color;
    switch (mood) {
    case PET_MOOD_HAPPY:  face_color = COLOR_YELLOW;                            break;
    case PET_MOOD_SAD:    face_color = COLOR(200, 200, 100); /* muted yellow */ break;
    case PET_MOOD_HUNGRY: face_color = COLOR(255, 200, 80);  /* orange-yellow */break;
    case PET_MOOD_SLEEPY: face_color = COLOR(180, 180, 120); /* dim yellow */   break;
    default:              face_color = COLOR_YELLOW;                            break;
    }
    draw_face_circle(face_color);
    draw_face_eyes(mood);
    draw_face_mouth(mood);
}

/* ── Stat bars (compact, labels to the left) ──────────────── */

static void draw_stat_bar(int16_t x, int16_t y, int16_t w, int16_t h,
                          int8_t value, const char *label, uint16_t bar_color) {
    /* label — 48px wide column */
    graphics_draw_text(x, y - 1, label, COLOR_WHITE, COLOR_BLACK, 0);

    /* bar background */
    int16_t bx = x + 48, bw = w - 48;
    graphics_fill_rect(bx, y, bw, h, COLOR_DARK_GRAY);

    /* filled portion */
    if (value > 0) {
        int fill_w = (int)bw * value / PET_STAT_MAX;
        if (fill_w < 1) fill_w = 1;
        graphics_fill_rect(bx, y, fill_w, h, bar_color);
    }
}

static void draw_stat_bars(const pet_stats_t *stats, const char *action_hint) {
    int16_t y = LAYOUT_STATBAR_Y;
    /* hint text above bars */
    graphics_fill_rect(10, y - 10, 220, 10, COLOR_BLACK);
    graphics_draw_text(10, y - 10, action_hint, COLOR_GRAY, COLOR_BLACK, 0);

    draw_stat_bar(10, y,      220, LAYOUT_STATBAR_H, stats->hunger,    "Hunger", COLOR_RED);
    draw_stat_bar(10, y + 9,  220, LAYOUT_STATBAR_H, stats->happiness, "Happy",  COLOR_GREEN);
    draw_stat_bar(10, y + 18, 220, LAYOUT_STATBAR_H, stats->energy,    "Energy", COLOR_BLUE);
}

/* ── Gamepad debug overlay ─────────────────────────────────── */
#define DEBUG_X     10
#define DEBUG_Y     LAYOUT_DEBUG_Y
#define DEBUG_W     220
#define DEBUG_H     LAYOUT_DEBUG_H

static const char *btn_names[] = {
    [CTRL_A] = "A", [CTRL_B] = "B", [CTRL_X] = "X", [CTRL_Y] = "Y",
    [CTRL_LB] = "LB", [CTRL_RB] = "RB",
    [CTRL_DPAD_UP] = "DU", [CTRL_DPAD_DOWN] = "DD",
    [CTRL_DPAD_LEFT] = "DL", [CTRL_DPAD_RIGHT] = "DR",
    [CTRL_START] = "St", [CTRL_SELECT] = "Se",
};

static void draw_debug_gamepad(const gamepad_state_t *gs) {
    char buf[64];
    int y = DEBUG_Y;
    graphics_fill_rect(DEBUG_X, y, DEBUG_W, DEBUG_H, COLOR_BLACK);
    graphics_draw_text(DEBUG_X, y, "--- Gamepad ---", COLOR_CYAN, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "L X:%+4d Y:%+4d", (int)gs->axis_x, (int)gs->axis_y);
    graphics_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "R X:%+4d Y:%+4d", (int)gs->axis_rx, (int)gs->axis_ry);
    graphics_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    snprintf(buf, sizeof(buf), "LT:%3d  RT:%3d", (int)gs->brake, (int)gs->throttle);
    graphics_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    static const ctrl_button_t row1[] = {CTRL_A, CTRL_B, CTRL_X, CTRL_Y, CTRL_LB, CTRL_RB};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row1[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row1[i]]);
        graphics_draw_text(DEBUG_X + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
    y += 8;

    static const ctrl_button_t row2[] = {CTRL_DPAD_UP, CTRL_DPAD_DOWN, CTRL_DPAD_LEFT,
                                         CTRL_DPAD_RIGHT, CTRL_START, CTRL_SELECT};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row2[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row2[i]]);
        graphics_draw_text(DEBUG_X + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
}

/* ── Draw static UI elements (title, mood bar, hints, gradient) ── */
static void draw_static_ui(void) {
    graphics_fill(COLOR_BLACK);

    /* title with pet name */
    char title[40];
    snprintf(title, sizeof(title), "ESP-PET  — %s", pet_get_name());
    graphics_draw_text(10, LAYOUT_TITLE_Y, title, COLOR_WHITE, COLOR_BLACK, 0);
    graphics_fill_rect(10, LAYOUT_UNDERLINE_Y, 220, 2, COLOR_WHITE);

    /* mood label bar */
    graphics_fill_rect(10, LAYOUT_MOOD_Y, 220, LAYOUT_MOOD_H, COLOR_DARK_GRAY);
    graphics_draw_text(16, LAYOUT_MOOD_Y + 5, "Mood:", COLOR_WHITE, COLOR_DARK_GRAY, 0);

    /* hints */
    graphics_draw_text(10, LAYOUT_HINT_Y,
        "Btn/A:action  B:talk  D-Pad L/R:cycle", COLOR_GREEN, COLOR_BLACK, 0);

    /* controller status */
    graphics_draw_text(10, LAYOUT_CTRL_Y, "Ctrl: searching...", COLOR_GRAY, COLOR_BLACK, 0);

    /* stat bars placeholder (filled by tick) */
    draw_stat_bars(pet_get_stats(), care_names[current_care]);

    /* rainbow bar */
    graphics_draw_rainbow_h(0, LAYOUT_GRAD_Y, DISPLAY_WIDTH, LAYOUT_GRAD_H);
}

/* ── Display task ──────────────────────────────────────────── */
static void display_task(void *param) {
    draw_static_ui();

    pet_mood_t last_mood = PET_MOOD_COUNT; /* force first draw */
    pet_stats_t last_stats = { -1, -1, -1, 0 };
    bool last_ctrl_conn = false;
    bool last_sleep = false;

    while (1) {
        controller_poll();

        /* ── Input: cycle care action (Btn/D-Pad) ──────────── */
        if (button_action_pressed() || controller_button_pressed(CTRL_A)) {
            bool ok = false;
            switch (current_care) {
            case 0: ok = pet_feed();  break;
            case 1: ok = pet_play();  break;
            case 2: ok = pet_sleep(); break;
            }
            if (ok) {
                ESP_LOGI(TAG, "Care '%s' applied", care_names[current_care]);
            }
        }

        if (controller_button_pressed(CTRL_DPAD_LEFT)) {
            current_care = (current_care + 2) % 3; /* wrap backwards */
        }
        if (controller_button_pressed(CTRL_DPAD_RIGHT)) {
            current_care = (current_care + 1) % 3;
        }

        /* ── Input: Talk button (random mood for now) ──────── */
        if (button_talk_pressed() || controller_button_pressed(CTRL_B)) {
            ESP_LOGI(TAG, "Talk pressed — AI response stub");
            /* TODO Phase 4: call AI API */
        }

        /* ── Read current pet state ────────────────────────── */
        pet_mood_t mood = pet_get_mood();
        const pet_stats_t *stats = pet_get_stats();
        bool sleeping = pet_is_sleeping();

        /* ── Redraw pet face on mood change ────────────────── */
        if (mood != last_mood || sleeping != last_sleep) {
            draw_pet_face(sleeping ? PET_MOOD_SLEEPY : mood);
            last_mood = mood;
            last_sleep = sleeping;
        }

        /* ── Redraw stat bars on stat change ───────────────── */
        if (stats->hunger != last_stats.hunger ||
            stats->happiness != last_stats.happiness ||
            stats->energy != last_stats.energy) {
            draw_stat_bars(stats, care_names[current_care]);
            last_stats = *stats;
        }

        /* ── Update mood label ─────────────────────────────── */
        {
            graphics_fill_rect(65, LAYOUT_MOOD_Y, 140, LAYOUT_MOOD_H, COLOR_DARK_GRAY);
            const char *label = sleeping ? "Sleeping" : mood_labels[mood];
            uint16_t lc = sleeping ? COLOR_CYAN :
                          (mood == PET_MOOD_HUNGRY ? COLOR_RED :
                           mood == PET_MOOD_SAD ? COLOR_BLUE : COLOR_YELLOW);
            graphics_draw_text(72, LAYOUT_MOOD_Y + 5, label, lc, COLOR_DARK_GRAY, 0);
        }

        /* ── Gamepad debug overlay ─────────────────────────── */
        bool ctrl_conn = controller_is_connected();
        if (ctrl_conn) {
            gamepad_state_t gs = controller_get_state();
            draw_debug_gamepad(&gs);
        } else if (last_ctrl_conn) {
            graphics_fill_rect(DEBUG_X, DEBUG_Y, DEBUG_W, DEBUG_H, COLOR_BLACK);
        }

        /* ── Connection status ─────────────────────────────── */
        if (ctrl_conn != last_ctrl_conn) {
            last_ctrl_conn = ctrl_conn;
            graphics_fill_rect(10, LAYOUT_CTRL_Y, 220, 10, COLOR_BLACK);
            graphics_draw_text(10, LAYOUT_CTRL_Y,
                ctrl_conn ? "Ctrl: connected" : "Ctrl: searching...",
                ctrl_conn ? COLOR_GREEN : COLOR_GRAY, COLOR_BLACK, 0);
        }

        display_flush();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void nimble_host_task(void *param) {
    nimble_port_run();   // never return
}

static void on_sync(void) {
    ESP_LOGI(TAG, "NimBLE Host synced, starting Xbox BLE scan...");
    xbox_ble_init(1);
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE Host reset (reason=%d)", reason);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP-PET ===");

    /* ── 1. NVS init (stores bonding keys) ─────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* ── 2. NimBLE Host config ─────────────────────────────── */
    nimble_port_init();

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    /* Security: Bonding + SC + Just Works */
    ble_hs_cfg.sm_bonding   = 1;
    ble_hs_cfg.sm_sc        = 1;
    ble_hs_cfg.sm_mitm      = 0;
    ble_hs_cfg.sm_io_cap    = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* ── 3. Peripherals ────────────────────────────────────── */
    display_init();

    buttons_init();
    controller_init();

    /* ── 4. Pet state (restores from NVS, starts tick timer) ─ */
    pet_init();

    /* ── 5. Display task ───────────────────────────────────── */
    xTaskCreate(display_task, "display", 8192, NULL, 5, NULL);

    /* ── 5. Start NimBLE Host task (does not return) ───────── */
    nimble_port_freertos_init(nimble_host_task);
}
