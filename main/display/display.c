#include "display.h"

#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "display";

// SPI host
#define LCD_HOST SPI2_HOST

// SPI clock speed (40 MHz)
#define LCD_PCLK_HZ (40 * 1000 * 1000)

// LEDC for backlight
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE  LEDC_LOW_SPEED_MODE
#define LEDC_CH    LEDC_CHANNEL_0
#define LEDC_FREQ  5000
#define LEDC_RES   LEDC_TIMER_8_BIT

// Panel handles
static esp_lcd_panel_io_handle_t io_handle = NULL;

// Framebuffer
static uint16_t *framebuffer = NULL;

// Strip height for flush (1/10 of display → ~11 KiB swap buffer)
#define FLUSH_STRIP_H (DISPLAY_HEIGHT / 10)

// DMA-safe swap buffer (allocated once in display_init)
static uint16_t *swap_buf = NULL;
static const uint8_t caset_cmd[4] = {0, 0, 0, (uint8_t)(DISPLAY_WIDTH - 1)};

// Skips flush when nothing changed
static bool dirty = false;

void display_mark_dirty(void) { dirty = true; }

// --- Backlight ---

void display_set_backlight(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)(percent * 255 / 100);
    ledc_set_duty(LEDC_MODE, LEDC_CH, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CH);
}

// --- Initialization ---

void display_init(void) {
    ESP_LOGI(TAG, "Initializing ST7789 display...");

    // Backlight PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = TFT_BL,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CH,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 255, // Full brightness initially
        .hpoint     = 0,
    };
    ledc_channel_config(&ledc_channel);

    // SPI bus
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = TFT_SCLK,
        .mosi_io_num     = TFT_MOSI,
        .miso_io_num     = -1, // Not used
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // Panel IO (SPI)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num     = TFT_CS,
        .dc_gpio_num     = TFT_DC,
        .spi_mode        = 0,
        .pclk_hz         = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));

    // Hardware reset
    gpio_set_direction(TFT_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // ST7789 init sequence
    // Sleep out
    esp_lcd_panel_io_tx_param(io_handle, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    uint8_t colmod = 0x55;  // COLMOD: 16-bit
    esp_lcd_panel_io_tx_param(io_handle, 0x3A, &colmod, 1);

    uint8_t madctl = 0x08;  // MADCTL: BGR order
    esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1);

    esp_lcd_panel_io_tx_param(io_handle, 0x21, NULL, 0);  // Inversion on
    esp_lcd_panel_io_tx_param(io_handle, 0x13, NULL, 0);  // Normal mode

    esp_lcd_panel_io_tx_param(io_handle, 0x29, NULL, 0);  // Display on
    vTaskDelay(pdMS_TO_TICKS(20));

    // Allocate framebuffer & swap buffer
    size_t fb_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    framebuffer = heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (framebuffer == NULL) {
        framebuffer = heap_caps_malloc(fb_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    }
    if (framebuffer == NULL) {
        framebuffer = malloc(fb_size);
    }
    assert(framebuffer != NULL);
    memset(framebuffer, 0, fb_size);

    swap_buf = heap_caps_malloc(DISPLAY_WIDTH * FLUSH_STRIP_H * sizeof(uint16_t),
                                MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    assert(swap_buf != NULL);

    ESP_LOGI(TAG, "Display initialized. FB at %p, size %zu", framebuffer, fb_size);
}

uint16_t *display_get_framebuffer(void) {
    return framebuffer;
}

// ── Internal: pixel address helper (used by flush, which operates directly on fb) ──

static inline uint16_t *fb_pixel(int16_t x, int16_t y) {
    return &framebuffer[y * DISPLAY_WIDTH + x];
}

// --- Flush to Display ---

void display_flush(void) {
    if (!dirty) return;

    esp_lcd_panel_io_tx_param(io_handle, 0x2A, caset_cmd, 4);

    for (int16_t y0 = 0; y0 < DISPLAY_HEIGHT; y0 += FLUSH_STRIP_H) {
        int16_t strip_h = FLUSH_STRIP_H;
        if (y0 + strip_h > DISPLAY_HEIGHT) strip_h = DISPLAY_HEIGHT - y0;

        uint8_t raset_cmd[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0,
                               (uint8_t)((y0 + strip_h - 1) >> 8),
                               (uint8_t)(y0 + strip_h - 1)};
        esp_lcd_panel_io_tx_param(io_handle, 0x2B, raset_cmd, 4);

        // Copy strip rows with 180° rotation + G↔B swap
        uint16_t *dst = swap_buf;
        for (int16_t row = DISPLAY_HEIGHT - 1 - y0;
             row > DISPLAY_HEIGHT - 1 - (y0 + strip_h); row--) {
            const uint16_t *src = fb_pixel(DISPLAY_WIDTH - 1, row);
            for (int16_t col = 0; col < DISPLAY_WIDTH; col++) {
                uint16_t c = *src--;
                *dst++ = (c & 0xF800) | ((c & 0x001F) << 6) | ((c >> 5) & 0x001F);
            }
        }

        size_t strip_bytes = (size_t)DISPLAY_WIDTH * strip_h * sizeof(uint16_t);
        esp_lcd_panel_io_tx_color(io_handle, 0x2C, (const void *)swap_buf, strip_bytes);
    }

    dirty = false;
}
