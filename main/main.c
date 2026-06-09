#include "buttons.h"
#include "gamepad/controller.h"
#include "display.h"
#include "graphics.h"
#include "pet/pet_state.h"
#include "pet/pet_sprite.h"
#include "ui/ui.h"
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

/* ── Display task: render loop ────────────────────────────────── */
static void display_task(void *param) {
    ui_draw_static();

    pet_mood_t last_mood = PET_MOOD_COUNT;
    pet_stats_t last_stats = { -1, -1, -1, 0 };
    bool last_ctrl_conn = false;
    bool last_sleep = false;

    while (1) {
        controller_poll();

        /* ── Input: Action button → current care ──────────── */
        if (button_action_pressed() || controller_button_pressed(CTRL_A)) {
            bool ok = false;
            switch (ui_care_index) {
            case 0: ok = pet_feed();  break;
            case 1: ok = pet_play();  break;
            case 2: ok = pet_sleep(); break;
            }
            if (ok) {
                ESP_LOGI(TAG, "Care '%s' applied", ui_care_names[ui_care_index]);
                ui_speech_show("Yay! Thank you!");
            }
        }

        /* ── D-Pad L/R: cycle care action ─────────────────── */
        if (controller_button_pressed(CTRL_DPAD_LEFT)) {
            ui_care_index = (ui_care_index + 2) % 3;
        }
        if (controller_button_pressed(CTRL_DPAD_RIGHT)) {
            ui_care_index = (ui_care_index + 1) % 3;
        }

        /* ── Talk button ──────────────────────────────────── */
        if (button_talk_pressed() || controller_button_pressed(CTRL_B)) {
            ESP_LOGI(TAG, "Talk pressed");
            if (ui_speech_visible()) {
                ui_speech_dismiss();
            } else {
                char ctx[PET_CONTEXT_MAX];
                pet_build_context_str(ctx, sizeof(ctx));
                ui_speech_show(ctx);
            }
        }

        /* ── Pet state ────────────────────────────────────── */
        pet_mood_t mood = pet_get_mood();
        const pet_stats_t *stats = pet_get_stats();
        bool sleeping = pet_is_sleeping();

        /* ── Redraw pet sprite on mood/sleep change ────────── */
        if (mood != last_mood || sleeping != last_sleep) {
            pet_sprite_draw(UI_PET_X, UI_PET_Y, sleeping ? PET_MOOD_SLEEPY : mood, sleeping);
            last_mood = mood;
            last_sleep = sleeping;
        } else {
            /* tick animation even when mood unchanged */
            pet_sprite_draw(UI_PET_X, UI_PET_Y, mood, sleeping);
        }

        /* ── Redraw stat bars ─────────────────────────────── */
        if (stats->hunger != last_stats.hunger ||
            stats->happiness != last_stats.happiness ||
            stats->energy != last_stats.energy) {
            ui_draw_stat_bars(stats);
            last_stats = *stats;
        }

        /* ── Mood label ───────────────────────────────────── */
        ui_draw_mood_label(mood, sleeping);

        /* ── Gamepad debug ────────────────────────────────── */
        bool ctrl_conn = controller_is_connected();
        if (ctrl_conn) {
            gamepad_state_t gs = controller_get_state();
            ui_draw_gamepad_debug(&gs);
        } else if (last_ctrl_conn) {
            ui_clear_gamepad_debug();
        }

        if (ctrl_conn != last_ctrl_conn) {
            last_ctrl_conn = ctrl_conn;
            ui_draw_ctrl_status(ctrl_conn);
        }

        /* ── Speech bubble overlay ────────────────────────── */
        ui_speech_render();

        display_flush();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── NimBLE task & callbacks ──────────────────────────────────── */
static void nimble_host_task(void *param) {
    nimble_port_run();
}

static void on_sync(void) {
    ESP_LOGI(TAG, "NimBLE Host synced, starting Xbox BLE scan...");
    xbox_ble_init(1);
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE Host reset (reason=%d)", reason);
}

/* ── Entry point ──────────────────────────────────────────────── */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP-PET ===");

    /* ── NVS ──────────────────────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* ── NimBLE ───────────────────────────────────────────── */
    nimble_port_init();
    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sm_bonding   = 1;
    ble_hs_cfg.sm_sc        = 1;
    ble_hs_cfg.sm_mitm      = 0;
    ble_hs_cfg.sm_io_cap    = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* ── Peripherals ──────────────────────────────────────── */
    display_init();
    buttons_init();
    controller_init();

    /* ── Pet state (restores NVS, starts decay timer) ─────── */
    pet_init();

    /* ── Display task ─────────────────────────────────────── */
    xTaskCreate(display_task, "display", 8192, NULL, 5, NULL);

    /* ── NimBLE host (does not return) ────────────────────── */
    nimble_port_freertos_init(nimble_host_task);
}
