#include "buttons.h"
#include "gamepad/controller.h"
#include "display.h"
#include "gamepad/xbox_ble.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── NimBLE native headers ────────────────────────────────────── */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *TAG = "main";

typedef enum {
    FACE_HAPPY,
    FACE_NEUTRAL,
    FACE_SAD,
    FACE_SURPRISED,
    FACE_COUNT
} face_expression_t;

static const char *face_labels[] = {
    [FACE_HAPPY]     = "Happy",
    [FACE_NEUTRAL]   = "Neutral",
    [FACE_SAD]       = "Sad",
    [FACE_SURPRISED] = "Surprised",
};

/* ── Layout zones (mutually exclusive, no overlap) ────────── */
#define LAYOUT_TITLE_Y       8
#define LAYOUT_UNDERLINE_Y   20
#define LAYOUT_HINT1_Y       30
#define LAYOUT_HINT2_Y       40
#define LAYOUT_EXPR_Y        50
#define LAYOUT_EXPR_H        18
#define LAYOUT_FACE_Y        72
#define LAYOUT_FACE_H        96
#define LAYOUT_DEBUG_Y       160
#define LAYOUT_DEBUG_H       50
#define LAYOUT_STATUS_Y      214
#define LAYOUT_GRAD_Y        232
#define LAYOUT_GRAD_H        8

/* ── Face (sits inside LAYOUT_FACE_Y … LAYOUT_FACE_Y+LAYOUT_FACE_H) ── */
#define FACE_CX  120
#define FACE_CY  120
#define FACE_R   26
#define EYE_Y    (FACE_CY - 8)
#define EYE_W    4
#define EYE_H    3
#define EYE_LX   110
#define EYE_RX   (2 * FACE_CX - EYE_LX - EYE_W + 1)

static void draw_face_circle(void) {
    int16_t r = FACE_R, cx = FACE_CX, cy = FACE_CY;
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                display_draw_pixel(cx + dx, cy + dy, COLOR_YELLOW);
            }
        }
    }
}

static void draw_smiley(face_expression_t expr) {
    int16_t sz = (FACE_R + 6) * 2;
    display_fill_rect(FACE_CX - (FACE_R + 6), FACE_CY - (FACE_R + 6),
                      sz, sz, COLOR_BLACK);
    draw_face_circle();

    switch (expr) {
    case FACE_HAPPY:
        display_fill_rect(EYE_LX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        display_fill_rect(EYE_RX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        {
            int16_t my = FACE_CY + 5;
            for (int16_t dx = -7; dx <= 7; dx++) {
                int16_t dy = -(dx * dx) / 9 + 3;
                display_draw_pixel(FACE_CX + dx, my + dy, COLOR_BLACK);
                display_draw_pixel(FACE_CX + dx, my + dy + 1, COLOR_BLACK);
            }
        }
        break;
    case FACE_NEUTRAL:
        display_fill_rect(EYE_LX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        display_fill_rect(EYE_RX, EYE_Y, EYE_W, EYE_H, COLOR_BLACK);
        display_fill_rect(FACE_CX - 5, FACE_CY + 8, 10, 2, COLOR_BLACK);
        break;
    case FACE_SAD:
        display_fill_rect(EYE_LX, EYE_Y + 1, EYE_W, EYE_H, COLOR_BLACK);
        display_fill_rect(EYE_RX, EYE_Y + 1, EYE_W, EYE_H, COLOR_BLACK);
        {
            int16_t my = FACE_CY + 14;
            for (int16_t dx = -7; dx <= 7; dx++) {
                int16_t dy = dx * dx / 9 - 3;
                display_draw_pixel(FACE_CX + dx, my + dy, COLOR_BLACK);
                display_draw_pixel(FACE_CX + dx, my + dy + 1, COLOR_BLACK);
            }
        }
        break;
    case FACE_SURPRISED:
        {
            int16_t ec = EYE_LX + EYE_W / 2;
            int16_t ecr = 2 * FACE_CX - ec;
            for (int16_t dy = -4; dy <= 4; dy++) {
                for (int16_t dx = -3; dx <= 3; dx++) {
                    if (dx * dx + dy * dy <= 16) {
                        display_draw_pixel(ec + dx, EYE_Y + dy, COLOR_BLACK);
                        display_draw_pixel(ecr + dx, EYE_Y + dy, COLOR_BLACK);
                    }
                }
            }
        }
        for (int16_t dy = -4; dy <= 4; dy++) {
            for (int16_t dx = -4; dx <= 4; dx++) {
                if (dx * dx + dy * dy <= 18) {
                    display_draw_pixel(FACE_CX + dx, FACE_CY + 8 + dy, COLOR_BLACK);
                }
            }
        }
        break;
    default:
        break;
    }
}

/* ── Gamepad debug overlay (sits inside LAYOUT_DEBUG_Y … LAYOUT_DEBUG_Y+LAYOUT_DEBUG_H) ── */
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

    /* clear debug area */
    display_fill_rect(DEBUG_X, y, DEBUG_W, DEBUG_H, COLOR_BLACK);

    /* row 0: title */
    display_draw_text(DEBUG_X, y, "--- Gamepad ---", COLOR_CYAN, COLOR_BLACK, 0);
    y += 8;

    /* row 1: L stick */
    snprintf(buf, sizeof(buf), "L X:%+4d Y:%+4d", (int)gs->axis_x, (int)gs->axis_y);
    display_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    /* row 2: R stick */
    snprintf(buf, sizeof(buf), "R X:%+4d Y:%+4d", (int)gs->axis_rx, (int)gs->axis_ry);
    display_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    /* row 3: triggers */
    snprintf(buf, sizeof(buf), "LT:%3d  RT:%3d", (int)gs->brake, (int)gs->throttle);
    display_draw_text(DEBUG_X, y, buf, COLOR_WHITE, COLOR_BLACK, 0);
    y += 8;

    /* row 4: A B X Y LB RB */
    static const ctrl_button_t row1[] = {CTRL_A, CTRL_B, CTRL_X, CTRL_Y, CTRL_LB, CTRL_RB};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row1[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row1[i]]);
        display_draw_text(DEBUG_X + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
    y += 8;

    /* row 5: DU DD DL DR St Se */
    static const ctrl_button_t row2[] = {CTRL_DPAD_UP, CTRL_DPAD_DOWN, CTRL_DPAD_LEFT,
                                         CTRL_DPAD_RIGHT, CTRL_START, CTRL_SELECT};
    for (int i = 0; i < 6; i++) {
        bool on = controller_button_is_pressed(row2[i]);
        snprintf(buf, sizeof(buf), "[%c]%-2s", on ? '*' : ' ', btn_names[row2[i]]);
        display_draw_text(DEBUG_X + i * 36, y, buf,
                          on ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 0);
    }
}

/* ── Draw the initial static UI (title, hints, expression bar, gradient) ── */
static void draw_static_ui(void) {
    display_fill(COLOR_BLACK);

    /* title */
    display_draw_text(10, LAYOUT_TITLE_Y, "ESP-PET", COLOR_WHITE, COLOR_BLACK, 0);
    display_fill_rect(10, LAYOUT_UNDERLINE_Y, 220, 2, COLOR_WHITE);

    /* hints */
    display_draw_text(10, LAYOUT_HINT1_Y, "Btn / A: next face", COLOR_GREEN, COLOR_BLACK, 0);
    display_draw_text(10, LAYOUT_HINT2_Y, "Talk / B: random",   COLOR_GREEN, COLOR_BLACK, 0);

    /* expression label bar */
    display_fill_rect(10, LAYOUT_EXPR_Y, 220, LAYOUT_EXPR_H, COLOR_DARK_GRAY);
    display_draw_text(16, LAYOUT_EXPR_Y + 5, "Expression:", COLOR_WHITE, COLOR_DARK_GRAY, 0);
    display_draw_text(98, LAYOUT_EXPR_Y + 5, face_labels[FACE_NEUTRAL],
                      COLOR_YELLOW, COLOR_DARK_GRAY, 0);

    /* controller status */
    display_draw_text(10, LAYOUT_STATUS_Y, "Ctrl: searching...", COLOR_GRAY, COLOR_BLACK, 0);

    /* bottom rainbow bar (full hue sweep left→right) */
    display_draw_rainbow_h(0, LAYOUT_GRAD_Y, DISPLAY_WIDTH, LAYOUT_GRAD_H);
}

static void display_task(void *param) {
    draw_static_ui();
    display_flush();

    face_expression_t current_face = FACE_NEUTRAL;
    draw_smiley(current_face);
    display_flush();

    bool last_ctrl_conn = false;

    while (1) {
        controller_poll();

        /* ── Input handling ──────────────────────────────────── */
        if (button_action_pressed() || controller_button_pressed(CTRL_A)) {
            current_face = (current_face + 1) % FACE_COUNT;
            ESP_LOGI(TAG, "Next -> %s", face_labels[current_face]);
        }

        if (button_talk_pressed() || controller_button_pressed(CTRL_B)) {
            current_face = (face_expression_t)(esp_random() % FACE_COUNT);
            ESP_LOGI(TAG, "Random -> %s", face_labels[current_face]);
        }

        /* ── Redraw face ─────────────────────────────────────── */
        draw_smiley(current_face);

        /* ── Update expression label ─────────────────────────── */
        {
            int ey = LAYOUT_EXPR_Y + 5;
            display_fill_rect(90, LAYOUT_EXPR_Y, 140, LAYOUT_EXPR_H, COLOR_DARK_GRAY);
            display_draw_text(98, ey, face_labels[current_face],
                              COLOR_YELLOW, COLOR_DARK_GRAY, 0);
        }

        /* ── Gamepad debug overlay (face zone is untouched) ──── */
        bool ctrl_conn = controller_is_connected();
        if (ctrl_conn) {
            gamepad_state_t gs = controller_get_state();
            draw_debug_gamepad(&gs);
        } else if (last_ctrl_conn) {
            /* Clear stale debug data when gamepad disconnects */
            display_fill_rect(DEBUG_X, DEBUG_Y, DEBUG_W, DEBUG_H, COLOR_BLACK);
        }

        /* ── Connection status ───────────────────────────────── */
        if (ctrl_conn != last_ctrl_conn) {
            last_ctrl_conn = ctrl_conn;
            display_fill_rect(10, LAYOUT_STATUS_Y, 220, 10, COLOR_BLACK);
            display_draw_text(10, LAYOUT_STATUS_Y,
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

    /* ── 4. Display task ───────────────────────────────────── */
    xTaskCreate(display_task, "display", 8192, NULL, 5, NULL);

    /* ── 5. Start NimBLE Host task (does not return) ───────── */
    nimble_port_freertos_init(nimble_host_task);
}
