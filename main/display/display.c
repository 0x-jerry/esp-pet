#include "display.h"
#include "graphics.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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
static esp_lcd_panel_handle_t panel_handle = NULL;

// Double-buffer ping-pong context
// ISR reads from the FIFO to know _which_ buffer's DMA just completed.
typedef struct {
    SemaphoreHandle_t sem[2];      // one binary semaphore per buffer
    int               pending[2];  // FIFO ring: buffer indices with DMA in flight
    volatile int      put;         // FIFO write cursor (render loop only)
    volatile int      get;         // FIFO read cursor  (ISR only)
} dbl_ctx_t;

static uint16_t *strip_buf[2] = {NULL, NULL};
static dbl_ctx_t dbl_ctx;
static uint32_t g_last_frame_us = 0;

// ISR: called when ANY color DMA transaction completes.
// Read from the FIFO to learn which buffer's semaphore to release.
static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                           esp_lcd_panel_io_event_data_t *edata,
                                           void *user_ctx) {
    dbl_ctx_t *ctx = (dbl_ctx_t *)user_ctx;
    int idx = ctx->pending[ctx->get];
    ctx->get = (ctx->get + 1) % 2;
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->sem[idx], &high_task_woken);
    return high_task_woken == pdTRUE;
}

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
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_STRIP_H * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // Create two binary semaphores, one per buffer. Both initially free.
    for (int i = 0; i < 2; i++) {
        dbl_ctx.sem[i] = xSemaphoreCreateBinary();
        assert(dbl_ctx.sem[i] != NULL);
        xSemaphoreGive(dbl_ctx.sem[i]);
    }
    dbl_ctx.put = 0;
    dbl_ctx.get = 0;

    // Panel IO (SPI)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num     = TFT_CS,
        .dc_gpio_num     = TFT_DC,
        .spi_mode        = 0,
        .pclk_hz         = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx        = &dbl_ctx,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));

    // Create ST7789 panel (handles init sequence, rotation, color order)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // Reset and initialize the panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // 180° rotation (mirror both axes)
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

    // Y offset of 80 pixels (shifts content downward)
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 80));

    // Inversion on (ST7789 typical)
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Allocate two DMA-safe strip buffers for ping-pong rendering
    for (int i = 0; i < 2; i++) {
        strip_buf[i] = spi_bus_dma_memory_alloc(LCD_HOST,
                              DISPLAY_WIDTH * DISPLAY_STRIP_H * sizeof(uint16_t), 0);
        assert(strip_buf[i] != NULL);
    }

    ESP_LOGI(TAG, "Display initialized. Strip bufs at %p / %p, size %zu each",
             strip_buf[0], strip_buf[1], DISPLAY_WIDTH * DISPLAY_STRIP_H * sizeof(uint16_t));
}

uint16_t *display_get_strip_buf(void) {
    return strip_buf[0];
}

// --- Strip-Based Frame Rendering (Double-Buffer Ping-Pong) ---
//
//  ping-pong timeline (two buffers, two semaphores, 2-slot FIFO):
//
//    Strip 0: Take sem[0] → draw  buf[0] → DMA buf[0] ──────────────┐
//    Strip 1:                Take sem[1] → draw  buf[1] → DMA buf[1] │ DMA runs
//             CPU drawing buf[1] while buf[0] is on the SPI bus      │ in bg
//    Strip 2: Take sem[0] ← ISR gives sem[0] ────────────────────────┘
//             …
static void render_frame(void (*render_cb)()) {
    int idx = 0;

    for (int16_t y0 = 0; y0 < DISPLAY_HEIGHT; y0 += DISPLAY_STRIP_H) {
        int16_t strip_h = DISPLAY_STRIP_H;
        if (y0 + strip_h > DISPLAY_HEIGHT) strip_h = DISPLAY_HEIGHT - y0;

        // Wait until buffer[idx] is free (its previous DMA completed)
        xSemaphoreTake(dbl_ctx.sem[idx], portMAX_DELAY);

        // Zero this strip in the current buffer
        memset(strip_buf[idx], 0, (size_t)DISPLAY_WIDTH * strip_h * sizeof(uint16_t));

        // Draw the strip into buffer[idx]
        graphics_begin_strip(strip_buf[idx], y0, strip_h);
        render_cb();
        graphics_end_strip();

        // Record which buffer is now in flight, then kick off DMA
        dbl_ctx.pending[dbl_ctx.put] = idx;
        dbl_ctx.put = (dbl_ctx.put + 1) % 2;
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle,
                          0, y0, DISPLAY_WIDTH, y0 + strip_h, strip_buf[idx]));

        // Ping-pong to the other buffer while DMA runs in background
        idx ^= 1;
    }

    // Wait for both in-flight DMAs to complete, then return tokens
    xSemaphoreTake(dbl_ctx.sem[0], portMAX_DELAY);
    xSemaphoreTake(dbl_ctx.sem[1], portMAX_DELAY);
    xSemaphoreGive(dbl_ctx.sem[0]);
    xSemaphoreGive(dbl_ctx.sem[1]);

    // Reset FIFO cursors for next frame
    dbl_ctx.put = 0;
    dbl_ctx.get = 0;
}

/**
 * Wait until the target frame interval has elapsed since the last call.
 * Call once per frame to cap the frame rate.
 *
 * @param target_fps  Target frames per second. Must be > 0.
 */
static void fps_limiter_wait(uint8_t target_fps) {
    static int64_t t_last_frame_end = 0;
    int64_t t_now = esp_timer_get_time();

    if (t_last_frame_end != 0) {
        uint32_t frame_interval_us = 1000000 / target_fps;
        int64_t elapsed = t_now - t_last_frame_end;

        if (elapsed < frame_interval_us) {
            uint32_t sleep_ms = (frame_interval_us - (uint32_t)elapsed) / 1000;
            if (sleep_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(sleep_ms));
            }
        }
    }
    t_last_frame_end = esp_timer_get_time();
}

void display_render_frame(void (*render_cb)(), uint8_t target_fps) {
    int64_t t0 = esp_timer_get_time();

    render_frame(render_cb);

    g_last_frame_us = (uint32_t)(esp_timer_get_time() - t0);

    if (target_fps > 0) {
        fps_limiter_wait(target_fps);
    }
}

uint32_t display_get_last_frame_us(void) {
    return g_last_frame_us;
}
