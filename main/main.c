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

static void draw_smiley(face_expression_t expr) {
    // Clear only the smiley area (leave text intact)
    int16_t smiley_y = 130;
    int16_t r = 30;

    // Erase previous face with a black rect
    int16_t erase_size = (r + 6) * 2;
    display_fill_rect(120 - (r + 6), smiley_y - (r + 6),
                      erase_size, erase_size, COLOR_BLACK);

    // Draw yellow circle base
    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                display_draw_pixel(120 + dx, smiley_y + dy, COLOR_YELLOW);
            }
        }
    }

    if (expr == FACE_SURPRISED) {
        // Big round eyes
        for (int16_t dy = -4; dy <= 4; dy++) {
            for (int16_t dx = -3; dx <= 3; dx++) {
                if (dx * dx + dy * dy <= 16) {
                    display_draw_pixel(120 - 10 + dx, smiley_y - 8 + dy, COLOR_BLACK);
                    display_draw_pixel(120 + 10 + dx, smiley_y - 8 + dy, COLOR_BLACK);
                }
            }
        }
        // Open mouth (circle)
        for (int16_t dy = -4; dy <= 4; dy++) {
            for (int16_t dx = -4; dx <= 4; dx++) {
                if (dx * dx + dy * dy <= 18) {
                    display_draw_pixel(120 + dx, smiley_y + 8 + dy, COLOR_BLACK);
                }
            }
        }
    } else if (expr == FACE_SAD) {
        // Sad eyes (shifted down at inner corners)
        display_fill_rect(120 - 10, smiley_y - 6, 3, 4, COLOR_BLACK);
        display_fill_rect(120 + 7, smiley_y - 6, 3, 4, COLOR_BLACK);
        // Frown mouth (arc below center)
        uint16_t mouth_y = smiley_y + 12;
        for (int16_t dx = -6; dx <= 6; dx++) {
            int16_t dy = dx * dx / 8 - 3;
            display_draw_pixel(120 + dx, mouth_y + dy, COLOR_BLACK);
            display_draw_pixel(120 + dx, mouth_y + dy + 1, COLOR_BLACK);
        }
    } else if (expr == FACE_HAPPY) {
        // Happy eyes (small arcs on top)
        display_fill_rect(120 - 10, smiley_y - 8, 4, 3, COLOR_BLACK);
        display_fill_rect(120 + 6, smiley_y - 8, 4, 3, COLOR_BLACK);
        // Smile mouth (arc above center)
        uint16_t mouth_y = smiley_y + 4;
        for (int16_t dx = -6; dx <= 6; dx++) {
            int16_t dy = -(dx * dx) / 8 + 3;
            display_draw_pixel(120 + dx, mouth_y + dy, COLOR_BLACK);
            display_draw_pixel(120 + dx, mouth_y + dy + 1, COLOR_BLACK);
        }
    } else {
        // Neutral face
        display_fill_rect(120 - 10, smiley_y - 6, 3, 3, COLOR_BLACK);
        display_fill_rect(120 + 7, smiley_y - 6, 3, 3, COLOR_BLACK);
        // Flat mouth
        display_fill_rect(120 - 5, smiley_y + 8, 10, 2, COLOR_BLACK);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP-PET: Smiley Button Demo ===");

    display_init();
    buttons_init();

    // Draw static UI frame
    display_fill(COLOR_BLACK);

    display_draw_text(10, 10, "ESP-PET", COLOR_WHITE, COLOR_BLACK, 0);
    display_fill_rect(10, 22, 220, 2, COLOR_WHITE);

    display_draw_text(10, 35, "BTN_L: cycle face", COLOR_GREEN, COLOR_BLACK, 0);
    display_draw_text(10, 45, "BTN_R: random face", COLOR_GREEN, COLOR_BLACK, 0);

    // Label area
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
            ESP_LOGI(TAG, "Action button pressed -> %s", face_labels[current_face]);
        }

        if (button_talk_pressed()) {
            current_face = (face_expression_t)(esp_random() % FACE_COUNT);
            redraw = true;
            ESP_LOGI(TAG, "Talk button pressed -> %s", face_labels[current_face]);
        }

        if (redraw) {
            draw_smiley(current_face);

            // Update expression label
            display_fill_rect(90, 60, 140, 30, COLOR_DARK_GRAY);
            display_draw_text(98, 68, face_labels[current_face],
                              COLOR_YELLOW, COLOR_DARK_GRAY, 0);

            display_flush();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
