#include "display.h"
#include "font.h"

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

static inline void mark_dirty(void) { dirty = true; }

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

// --- Drawing Primitives ---

static inline uint16_t *fb_pixel(int16_t x, int16_t y) {
    return &framebuffer[y * DISPLAY_WIDTH + x];
}

uint16_t *display_get_framebuffer(void) {
    return framebuffer;
}

void display_fill(uint16_t color) {
    size_t count = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    for (size_t i = 0; i < count; i++) {
        framebuffer[i] = color;
    }
    mark_dirty();
}

void display_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    *fb_pixel(x, y) = color;
    mark_dirty();
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    uint16_t *p_row = fb_pixel(x, y);
    for (int16_t row = 0; row < h; row++) {
        uint16_t *p = p_row;
        for (int16_t col = 0; col < w; col++, p++) {
            *p = color;
        }
        p_row += DISPLAY_WIDTH;
    }
    mark_dirty();
}

void display_draw_sprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    int16_t start_x = (x < 0) ? -x : 0;
    int16_t start_y = (y < 0) ? -y : 0;
    int16_t end_x = (x + w > DISPLAY_WIDTH)  ? DISPLAY_WIDTH - x : w;
    int16_t end_y = (y + h > DISPLAY_HEIGHT) ? DISPLAY_HEIGHT - y : h;

    for (int16_t row = start_y; row < end_y; row++) {
        for (int16_t col = start_x; col < end_x; col++) {
            uint16_t color = data[row * w + col];
            *fb_pixel(x + col, y + row) = color;
        }
    }
    mark_dirty();
}

// --- Text Rendering ---

int display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || x + FONT_WIDTH < 0 || y + FONT_HEIGHT < 0) {
        return x + FONT_WIDTH;
    }

    const uint8_t *glyph = font_get_char(c);

    for (int16_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = glyph[col];
        int16_t px = x + col;
        if (px < 0 || px >= DISPLAY_WIDTH) continue;

        uint16_t *p = fb_pixel(px, y);
        for (int16_t row = 0; row < FONT_HEIGHT; row++, p += DISPLAY_WIDTH) {
            int16_t py = y + row;
            if (py >= 0 && py < DISPLAY_HEIGHT) {
                *p = (line & (1 << row)) ? color : bg_color;
            }
        }
    }

    mark_dirty();
    return x + FONT_WIDTH;
}

/**
 * @brief Render a string on the framebuffer (supports newline / word wrap)
 *
 * Walks through the string character-by-character and calls display_draw_char()
 * for each one.  Newlines ('\n') force a line break.  If max_width is positive,
 * text is automatically wrapped at word boundaries when possible, or at character
 * boundaries as a fallback.
 *
 * @param x         Starting column (top-left)
 * @param y         Starting row (top-left)
 * @param text      Null-terminated ASCII string to draw
 * @param color     Character colour (RGB565)
 * @param bg_color  Background colour (RGB565)
 * @param max_width Maximum pixel width before automatic wrapping (0 = no wrap)
 * @return int      The final y + FONT_HEIGHT, useful for laying out the next line
 */
int display_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width) {
    int16_t cur_x = x;
    int16_t cur_y = y;

    while (*text) {
        char c = *text;

        /* ---- 1. Explicit newline ---- */
        if (c == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT;
            text++;
            continue;
        }

        /* ---- 2. Word wrap: peek ahead at the next word on a space ---- */
        if (c == ' ' && max_width > 0) {
            const char *p = text + 1;
            int word_w = 0;
            // Measure the pixel width of the next word (until space / newline / end)
            while (*p && *p != ' ' && *p != '\n') {
                word_w += FONT_WIDTH;
                p++;
            }
            // If the current line can't fit the next word, wrap
            if (cur_x + word_w > x + max_width) {
                cur_x = x;
                cur_y += FONT_HEIGHT;
                text++;
                continue;
            }
        }

        /* ---- 3. Character-level line wrap ---- */
        if (max_width > 0 && cur_x + FONT_WIDTH > x + max_width) {
            cur_x = x;
            cur_y += FONT_HEIGHT;
        }

        /* ---- 4. Draw the current character ---- */
        cur_x = display_draw_char(cur_x, cur_y, c, color, bg_color);
        text++;
    }

    return cur_y + FONT_HEIGHT;
}

// --- Rainbow ---

static uint16_t hue_to_rgb565(int hue) {
    int h = hue % 360;
    int sector = h / 60;
    int fract  = h - sector * 60;
    int p = 0;
    int q = (255 * (60 - fract) + 30) / 60;
    int t = (255 * fract + 30) / 60;
    int r, g, b;

    switch (sector) {
        case 0: r = 255; g = t;   b = p;   break;
        case 1: r = q;   g = 255; b = p;   break;
        case 2: r = p;   g = 255; b = t;   break;
        case 3: r = p;   g = q;   b = 255; break;
        case 4: r = t;   g = p;   b = 255; break;
        default: r = 255; g = p;   b = q;   break;
    }

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void display_draw_rainbow_h(int16_t x, int16_t y, int16_t w, int16_t h) {
    // Clip
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    int16_t denom = (w > 1) ? (w - 1) : 1;
    for (int16_t col = 0; col < w; col++) {
        int hue = (360 * col) / denom;          // 0 → 360 across width
        uint16_t color = hue_to_rgb565(hue);
        uint16_t *p = fb_pixel(x + col, y);
        for (int16_t row = 0; row < h; row++) {
            *p = color;
            p += DISPLAY_WIDTH;
        }
    }
    mark_dirty();
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
