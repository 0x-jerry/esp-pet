#include "display.h"
#include "font.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
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

// Framebuffer
static uint16_t *framebuffer = NULL;

// Dirty rectangle tracking for partial flush
static int16_t dirty_x1, dirty_y1, dirty_x2, dirty_y2;
static bool dirty = false;

static void mark_dirty(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t x2 = x + w - 1;
    int16_t y2 = y + h - 1;
    if (!dirty) {
        dirty_x1 = x;
        dirty_y1 = y;
        dirty_x2 = x2;
        dirty_y2 = y2;
        dirty = true;
    } else {
        if (x < dirty_x1) dirty_x1 = x;
        if (y < dirty_y1) dirty_y1 = y;
        if (x2 > dirty_x2) dirty_x2 = x2;
        if (y2 > dirty_y2) dirty_y2 = y2;
    }
    // Clamp to display bounds
    if (dirty_x1 < 0) dirty_x1 = 0;
    if (dirty_y1 < 0) dirty_y1 = 0;
    if (dirty_x2 >= DISPLAY_WIDTH) dirty_x2 = DISPLAY_WIDTH - 1;
    if (dirty_y2 >= DISPLAY_HEIGHT) dirty_y2 = DISPLAY_HEIGHT - 1;
}

static void mark_all_dirty(void) {
    dirty_x1 = 0;
    dirty_y1 = 0;
    dirty_x2 = DISPLAY_WIDTH - 1;
    dirty_y2 = DISPLAY_HEIGHT - 1;
    dirty = true;
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

    // 1. Configure backlight PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = PIN_BL,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CH,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 255, // Full brightness initially
        .hpoint     = 0,
    };
    ledc_channel_config(&ledc_channel);

    // 2. Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = PIN_SCLK,
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = -1, // Not used
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // 3. Create panel IO (SPI)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num     = PIN_CS,
        .dc_gpio_num     = PIN_DC,
        .spi_mode        = 0,
        .pclk_hz         = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));

    // 4. Create ST7789 panel
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &panel_handle));

    // 5. Initialize panel
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    // 6. Set orientation (adjust if display is rotated)
    esp_lcd_panel_mirror(panel_handle, true, false); // mirror x if needed
    esp_lcd_panel_swap_xy(panel_handle, false);

    // 7. Turn on display
    esp_lcd_panel_disp_on_off(panel_handle, true);

    // 8. Allocate framebuffer
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

    ESP_LOGI(TAG, "Display initialized. FB at %p, size %zu", framebuffer, fb_size);
}

// --- Drawing Primitives ---

uint16_t *display_get_framebuffer(void) {
    return framebuffer;
}

void display_fill(uint16_t color) {
    size_t count = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    for (size_t i = 0; i < count; i++) {
        framebuffer[i] = color;
    }
    mark_all_dirty();
}

void display_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    framebuffer[y * DISPLAY_WIDTH + x] = color;
    mark_dirty(x, y, 1, 1);
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Clip
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int16_t row = y; row < y + h; row++) {
        for (int16_t col = x; col < x + w; col++) {
            framebuffer[row * DISPLAY_WIDTH + col] = color;
        }
    }
    mark_dirty(x, y, w, h);
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
            framebuffer[(y + row) * DISPLAY_WIDTH + (x + col)] = color;
        }
    }
    mark_dirty(x, y, w, h);
}

// --- Text Rendering ---

int display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || x + FONT_WIDTH < 0 || y + FONT_HEIGHT < 0) {
        return x + FONT_WIDTH;
    }

    const uint8_t *glyph = font_get_char(c);

    for (int16_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = glyph[col];
        for (int16_t row = 0; row < FONT_HEIGHT; row++) {
            int16_t px = x + col;
            int16_t py = y + row;
            if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
                framebuffer[py * DISPLAY_WIDTH + px] = (line & (1 << row)) ? color : bg_color;
            }
        }
    }

    mark_dirty(x, y, FONT_WIDTH, FONT_HEIGHT);
    return x + FONT_WIDTH;
}

int display_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg_color, int16_t max_width) {
    int16_t cur_x = x;
    int16_t cur_y = y;

    while (*text) {
        char c = *text;

        // Newline
        if (c == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT;
            text++;
            continue;
        }

        // Word wrap: check if next word fits
        if (c == ' ' && max_width > 0) {
            // Look ahead to measure next word
            const char *p = text + 1;
            int word_w = 0;
            while (*p && *p != ' ' && *p != '\n') {
                word_w += FONT_WIDTH;
                p++;
            }
            if (cur_x + word_w > x + max_width) {
                cur_x = x;
                cur_y += FONT_HEIGHT;
                text++;
                continue;
            }
        }

        // Character wrap at end of line
        if (max_width > 0 && cur_x + FONT_WIDTH > x + max_width) {
            cur_x = x;
            cur_y += FONT_HEIGHT;
        }

        cur_x = display_draw_char(cur_x, cur_y, c, color, bg_color);
        text++;
    }

    return cur_y + FONT_HEIGHT;
}

// --- Flush to Display ---

void display_flush(void) {
    if (!dirty) return;

    int16_t x1 = dirty_x1;
    int16_t y1 = dirty_y1;
    int16_t w  = dirty_x2 - dirty_x1 + 1;
    int16_t h  = dirty_y2 - dirty_y1 + 1;

    // Align x to even for better performance
    if (x1 & 1) {
        x1--;
        w++;
    }

    // Send dirty region to display
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x1 + w, y1 + h,
        (const void *)(framebuffer + y1 * DISPLAY_WIDTH + x1));

    dirty = false;
}
