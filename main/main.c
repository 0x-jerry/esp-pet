#include "buttons.h"
#include "display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

#define FACE_CX  120
#define FACE_CY  130
#define FACE_R   30
#define EYE_Y    (FACE_CY - 8)
#define EYE_W    4
#define EYE_H    3
#define EYE_LX   110
#define EYE_RX   (2 * FACE_CX - EYE_LX - EYE_W + 1)   // 127 — mirrors EYE_LX around FACE_CX

static void draw_face_circle(void) {
    int16_t r = FACE_R;
    int16_t cx = FACE_CX, cy = FACE_CY;
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                display_draw_pixel(cx + dx, cy + dy, COLOR_YELLOW);
            }
        }
    }
}

static void draw_smiley(face_expression_t expr) {
    // erase face region
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
            int16_t e_center_l = EYE_LX + EYE_W / 2;
            int16_t e_center_r = 2 * FACE_CX - e_center_l;
            for (int16_t dy = -4; dy <= 4; dy++) {
                for (int16_t dx = -3; dx <= 3; dx++) {
                    if (dx * dx + dy * dy <= 16) {
                        display_draw_pixel(e_center_l + dx, EYE_Y + dy, COLOR_BLACK);
                        display_draw_pixel(e_center_r + dx, EYE_Y + dy, COLOR_BLACK);
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

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP-PET: Smiley Button Demo ===");

    display_init();
    buttons_init();

    display_fill(COLOR_BLACK);

    display_draw_text(10, 10, "ESP-PET", COLOR_WHITE, COLOR_BLACK, 0);
    display_fill_rect(10, 22, 220, 2, COLOR_WHITE);
    display_draw_text(10, 35, "BTN_L: cycle face", COLOR_GREEN, COLOR_BLACK, 0);
    display_draw_text(10, 45, "BTN_R: random face", COLOR_GREEN, COLOR_BLACK, 0);

    display_fill_rect(10, 60, 220, 30, COLOR_DARK_GRAY);
    display_draw_text(16, 68, "Expression:", COLOR_WHITE, COLOR_DARK_GRAY, 0);
    display_draw_text(98, 68, face_labels[FACE_NEUTRAL], COLOR_YELLOW, COLOR_DARK_GRAY, 0);
    display_flush();

    face_expression_t current_face = FACE_NEUTRAL;
    draw_smiley(current_face);
    display_flush();

    while (1) {
        bool redraw = false;

        if (button_action_pressed()) {
            current_face = (current_face + 1) % FACE_COUNT;
            redraw = true;
            ESP_LOGI(TAG, "Action -> %s", face_labels[current_face]);
        }
        if (button_talk_pressed()) {
            current_face = (face_expression_t)(esp_random() % FACE_COUNT);
            redraw = true;
            ESP_LOGI(TAG, "Talk -> %s", face_labels[current_face]);
        }

        if (redraw) {
            draw_smiley(current_face);
            display_fill_rect(90, 60, 140, 30, COLOR_DARK_GRAY);
            display_draw_text(98, 68, face_labels[current_face],
                              COLOR_YELLOW, COLOR_DARK_GRAY, 0);
            display_flush();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
