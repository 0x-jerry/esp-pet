#include "my_platform.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "bp32";

static SemaphoreHandle_t g_mutex;
static uni_gamepad_t g_gamepad;
static bool g_connected;

struct uni_platform *get_my_platform(void);

static void platform_init(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    g_mutex = xSemaphoreCreateMutex();
    memset(&g_gamepad, 0, sizeof(g_gamepad));
    g_connected = false;
    ESP_LOGI(TAG, "Bluepad32 platform init");
}

static void platform_on_init_complete(void) {
    ESP_LOGI(TAG, "Ready, starting scan...");
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static uni_error_t platform_on_device_discovered(bd_addr_t addr,
                                                  const char *name,
                                                  uint16_t cod, uint8_t rssi) {
    if (name) {
        ESP_LOGI(TAG, "Discovered: '%s'", name);
    }
    return UNI_ERROR_SUCCESS;
}

static void platform_on_device_connected(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device connected");
}

static void platform_on_device_disconnected(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device disconnected");
    g_connected = false;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    memset(&g_gamepad, 0, sizeof(g_gamepad));
    xSemaphoreGive(g_mutex);
}

static uni_error_t platform_on_device_ready(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device ready!");
    g_connected = true;
    return UNI_ERROR_SUCCESS;
}

static void platform_on_controller_data(uni_hid_device_t *d,
                                         uni_controller_t *ctl) {
    if (ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_gamepad = ctl->gamepad;
        xSemaphoreGive(g_mutex);
    }
}

static const uni_property_t *platform_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void platform_on_oob_event(uni_platform_oob_event_t event, void *data) {
    switch (event) {
    case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
        ESP_LOGI(TAG, "Bluetooth enabled: %d", (bool)(data));
        break;
    default:
        break;
    }
}

struct uni_platform *get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "esp-pet",
        .init = platform_init,
        .on_init_complete = platform_on_init_complete,
        .on_device_discovered = platform_on_device_discovered,
        .on_device_connected = platform_on_device_connected,
        .on_device_disconnected = platform_on_device_disconnected,
        .on_device_ready = platform_on_device_ready,
        .on_oob_event = platform_on_oob_event,
        .on_controller_data = platform_on_controller_data,
        .get_property = platform_get_property,
    };
    return &plat;
}

// --- Public accessors ---

void my_platform_register(void) {
    uni_platform_set_custom(get_my_platform());
}

uni_gamepad_t my_platform_get_gamepad(void) {
    uni_gamepad_t gp;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    gp = g_gamepad;
    xSemaphoreGive(g_mutex);
    return gp;
}

bool my_platform_is_connected(void) {
    return g_connected;
}
