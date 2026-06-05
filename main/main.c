#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP-PET Phase 1: Display Test ===");

    // Initialize display
    display_init();

    // --- Test Pattern ---

    // 1. Fill entire screen with black
    display_fill(COLOR_BLACK);
    display_flush();

    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Draw colored rectangles (test fill_rect)
    display_fill_rect(0, 0, 80, 80, COLOR_RED);
    display_fill_rect(80, 0, 80, 80, COLOR_GREEN);
    display_fill_rect(160, 0, 80, 80, COLOR_BLUE);
    display_flush();

    vTaskDelay(pdMS_TO_TICKS(500));

    display_fill_rect(0, 80, 80, 80, COLOR_YELLOW);
    display_fill_rect(80, 80, 80, 80, COLOR_CYAN);
    display_fill_rect(160, 80, 80, 80, COLOR_MAGENTA);
    display_flush();

    vTaskDelay(pdMS_TO_TICKS(500));

    display_fill_rect(0, 160, 80, 80, COLOR_WHITE);
    display_fill_rect(80, 160, 80, 80, COLOR_GRAY);
    display_fill_rect(160, 160, 80, 80, COLOR_DARK_GRAY);
    display_flush();

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 3. Clear and draw text
    display_fill(COLOR_BLACK);

    // Title
    display_draw_text(10, 10, "ESP-PET", COLOR_WHITE, COLOR_BLACK, 0);
    display_draw_text(10, 22, "Display Test", COLOR_CYAN, COLOR_BLACK, 0);

    // Separator line
    display_fill_rect(10, 34, 220, 2, COLOR_WHITE);

    // Hardware info
    display_draw_text(10, 45, "MCU: ESP32-C6", COLOR_GREEN, COLOR_BLACK, 0);
    display_draw_text(10, 55, "LCD: ST7789 240x240", COLOR_GREEN, COLOR_BLACK, 0);

    // Color test bars
    display_fill_rect(10, 75, 220, 20, COLOR_RED);
    display_draw_text(10, 78, "RED", COLOR_WHITE, COLOR_RED, 0);

    display_fill_rect(10, 100, 220, 20, COLOR_GREEN);
    display_draw_text(10, 103, "GREEN", COLOR_BLACK, COLOR_GREEN, 0);

    display_fill_rect(10, 125, 220, 20, COLOR_BLUE);
    display_draw_text(10, 128, "BLUE", COLOR_WHITE, COLOR_BLUE, 0);

    // Pixel art test: a simple smiley face using draw_pixel
    int cx = 120, cy = 185, r = 15;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                display_draw_pixel(cx + dx, cy + dy, COLOR_YELLOW);
            }
        }
    }
    // Eyes
    display_fill_rect(cx - 7, cy - 4, 4, 4, COLOR_BLACK);
    display_fill_rect(cx + 3, cy - 4, 4, 4, COLOR_BLACK);
    // Mouth
    display_fill_rect(cx - 5, cy + 5, 10, 2, COLOR_BLACK);

    // Done message
    display_draw_text(10, 215, "Phase 1 OK!", COLOR_WHITE, COLOR_BLACK, 0);

    display_flush();

    ESP_LOGI(TAG, "Test pattern displayed. Phase 1 complete.");
}
