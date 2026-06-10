/**
 * @file gfx_init.c
 * @brief esp_emote_gfx initialization with ST7789 flush bridge.
 */

#include "gfx_init.h"
#include "display.h"

#include "gfx.h"
#include "esp_log.h"

static const char *TAG = "gfx";

static gfx_handle_t g_gfx_handle = NULL;

/* ── Flush callback ───────────────────────────────────────────── */

static void gfx_flush_cb(gfx_disp_t *disp, int x1, int y1, int x2, int y2,
                         const void *data) {
    esp_lcd_panel_handle_t panel =
        (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
}

/* ── Public API ───────────────────────────────────────────────── */

gfx_disp_t *gfx_init(void) {
    gfx_core_config_t gfx_cfg = {
        .fps  = 30,
        .task = GFX_EMOTE_INIT_CONFIG(),
    };
    g_gfx_handle = gfx_emote_init(&gfx_cfg);
    if (g_gfx_handle == NULL) {
        ESP_LOGE(TAG, "gfx_emote_init failed");
        return NULL;
    }

    gfx_disp_config_t disp_cfg = {
        .h_res     = DISPLAY_WIDTH,
        .v_res     = DISPLAY_HEIGHT,
        .flush_cb  = gfx_flush_cb,
        .update_cb = NULL,
        .user_data = (void *)display_get_panel_handle(),
        .flags     = { .swap = false },
        .buffers   = { .buf1 = NULL, .buf2 = NULL,
                       .buf_pixels = DISPLAY_WIDTH * 16 },
    };
    gfx_disp_t *disp = gfx_disp_add(g_gfx_handle, &disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "gfx_disp_add failed");
        gfx_emote_deinit(g_gfx_handle);
        g_gfx_handle = NULL;
        return NULL;
    }

    ESP_LOGI(TAG, "Initialized (30 fps)");
    return disp;
}

void gfx_deinit(gfx_handle_t handle) {
    if (handle) {
        gfx_emote_deinit(handle);
    }
    g_gfx_handle = NULL;
}
