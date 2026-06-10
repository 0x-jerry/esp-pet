/**
 * @file display.c
 * @brief ST7789 hardware initialization for ESP32-C6.
 *
 * Initializes SPI bus, LCD panel, and backlight PWM.
 * Framebuffer management is handled by esp_emote_gfx.
 */

#include "display.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_log.h"

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

// --- Backlight ---

void display_set_backlight(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)(percent * 255 / 100);
    ledc_set_duty(LEDC_MODE, LEDC_CH, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CH);
}

// --- Panel handle accessor ---

esp_lcd_panel_handle_t display_get_panel_handle(void) {
    return panel_handle;
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

    ESP_LOGI(TAG, "Display initialized");
}
