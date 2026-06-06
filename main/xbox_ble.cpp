/**
 * xbox_ble.cpp — NimBLE-based Xbox Wireless Controller BLE HID host
 *
 * Implements the full connection chain:
 *   Scan → Connect → Secure Pairing (Bonding+MITM+SC) → Discover HID → Subscribe → Parse
 *
 * Uses ESP-IDF's built-in NimBLE stack (NimBLEDevice C++ API).
 * The public API is `extern "C"` so that main.c / controller.c can call it directly.
 */

#include "xbox_ble.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "NimBLEDevice.h"
#include "NimBLEScan.h"
#include "NimBLEAdvertisedDevice.h"
#include "NimBLEClient.h"
#include "NimBLERemoteService.h"
#include "NimBLERemoteCharacteristic.h"
#include "NimBLEConnInfo.h"

#define TAG "xbox_ble"

// BLE UUIDs
static const NimBLEUUID HID_SERVICE_UUID("1812");
static const NimBLEUUID HID_REPORT_CHAR_UUID("2A4D");

// Scan / timing
static constexpr uint32_t SCAN_DURATION_MS   = 5000;
static constexpr uint32_t RECONNECT_DELAY_MS = 500;

// Xbox BLE input report: 1-byte report ID + 16-byte payload
static constexpr size_t XBOX_REPORT_MIN_LEN = 17;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static SemaphoreHandle_t  g_mutex     = nullptr;
static xbox_gamepad_t     g_gamepad   = {};
static bool               g_connected = false;
static NimBLEClient*      g_client    = nullptr;

// Forward declarations
static void start_scanning();
static void connect_to_device(const NimBLEAdvertisedDevice& dev);
static void discover_hid_service(NimBLEClient *client);

// ===========================================================================
// Scan callbacks
// ===========================================================================

class XboxScanCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice *pDev) override {
        if (g_connected) return;
        if (!pDev->haveName()) return;

        std::string name = pDev->getName();
        ESP_LOGD(TAG, "Scan: %s (RSSI=%d)", name.c_str(), pDev->getRSSI());

        if (name.find("Xbox") == std::string::npos &&
            name.find("xbox") == std::string::npos) {
            return;
        }

        ESP_LOGI(TAG, "🎮 Found Xbox controller: %s [%s] (RSSI=%d)",
                 name.c_str(),
                 pDev->getAddress().toString().c_str(),
                 pDev->getRSSI());

        // Stop scanning and connect
        NimBLEDevice::getScan()->stop();
        connect_to_device(*pDev);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        ESP_LOGI(TAG, "Scan ended (%d results, reason=%d)",
                 results.getCount(), reason);
        if (!g_connected && results.getCount() == 0) {
            ESP_LOGW(TAG, "No devices found, re-scanning...");
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            start_scanning();
        }
    }
};

// ===========================================================================
// Client callbacks
// ===========================================================================

class XboxClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient *pClient) override {
        ESP_LOGI(TAG, "✅ Connected");

        if (!pClient->secureConnection()) {
            ESP_LOGE(TAG, "❌ secureConnection() failed");
            pClient->disconnect();
            return;
        }
        ESP_LOGI(TAG, "🔐 Secure connection requested, waiting for pairing...");
    }

    void onConnectFail(NimBLEClient *pClient, int reason) override {
        ESP_LOGW(TAG, "❌ Connection failed (reason=%d)", reason);
        g_connected = false;
        g_client = nullptr;
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        start_scanning();
    }

    void onDisconnect(NimBLEClient *pClient, int reason) override {
        ESP_LOGI(TAG, "Disconnected (reason=%d)", reason);
        g_connected = false;
        g_client = nullptr;
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        start_scanning();
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (!connInfo.isEncrypted()) {
            ESP_LOGE(TAG, "❌ Auth complete but NOT encrypted!");
            return;
        }
        ESP_LOGI(TAG, "🔐 Authentication complete — link encrypted!");
        discover_hid_service(g_client);
    }
};

// ===========================================================================
// HID Report notification callback
// ===========================================================================

static void hid_report_notify_cb(NimBLERemoteCharacteristic *pChr,
                                 uint8_t *data, size_t len,
                                 bool isNotify) {
    if (!isNotify) return;
    if (len < XBOX_REPORT_MIN_LEN) return;

    // Debug: dump raw bytes
    ESP_LOGD(TAG, "HID Report [%d]: %02X %02X %02X %02X %02X %02X %02X %02X ...",
             len, data[0], data[1], data[2], data[3],
             data[4], data[5], data[6], data[7]);

    if (data[0] != 0x01) {
        if (data[0] == 0x04) ESP_LOGI(TAG, "🔋 Battery: %d%%", data[1]);
        return;
    }

    xbox_gamepad_t gp = {};

    // Axis (little-endian 16-bit, center ≈ 32767)
    gp.axis_x   = (int32_t)(data[1]  | (data[2]  << 8));
    gp.axis_y   = (int32_t)(data[3]  | (data[4]  << 8));
    gp.axis_rx  = (int32_t)(data[5]  | (data[6]  << 8));
    gp.axis_ry  = (int32_t)(data[7]  | (data[8]  << 8));

    // Triggers (little-endian, lower 10 bits)
    gp.brake    = (int32_t)((data[9]  | (data[10] << 8)) & 0x03FF);
    gp.throttle = (int32_t)((data[11] | (data[12] << 8)) & 0x03FF);

    // D-pad hat switch
    gp.dpad     = data[13] & 0x0F;

    // Button bitmap (16 bits)
    gp.buttons  = (uint16_t)(data[14] | (data[15] << 8));

    // Publish
    if (g_mutex) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_gamepad = gp;
        xSemaphoreGive(g_mutex);
    }
}

// ===========================================================================
// GATT service discovery
// ===========================================================================

static void discover_hid_service(NimBLEClient *client) {
    ESP_LOGI(TAG, "Discovering HID service...");

    NimBLERemoteService *hidSvc = client->getService(HID_SERVICE_UUID);
    if (!hidSvc) {
        ESP_LOGE(TAG, "❌ HID service not found!");
        client->disconnect();
        return;
    }
    ESP_LOGI(TAG, "✅ HID service found");

    auto chars = hidSvc->getCharacteristics(true);
    for (auto chr : chars) {
        NimBLEUUID uuid = chr->getUUID();

        ESP_LOGI(TAG, "  Char: %s notify=%d read=%d write=%d",
                 uuid.toString().c_str(),
                 chr->canNotify(), chr->canRead(), chr->canWrite());

        if (uuid == HID_REPORT_CHAR_UUID && chr->canNotify()) {
            if (chr->subscribe(true, hid_report_notify_cb)) {
                ESP_LOGI(TAG, "✅ Subscribed to HID Report");
                g_connected = true;
            } else {
                ESP_LOGE(TAG, "❌ Failed to subscribe");
                client->disconnect();
            }
            return;
        }
    }

    ESP_LOGE(TAG, "❌ HID Report char not found");
    client->disconnect();
}

// ===========================================================================
// Connection flow
// ===========================================================================

static void connect_to_device(const NimBLEAdvertisedDevice& dev) {
    if (g_connected) return;

    ESP_LOGI(TAG, "Connecting to %s (%s)...",
             dev.getAddress().toString().c_str(), dev.getName().c_str());

    NimBLEClient *client = NimBLEDevice::createClient();
    if (!client) {
        ESP_LOGE(TAG, "❌ Failed to create client");
        start_scanning();
        return;
    }

    client->setClientCallbacks(new XboxClientCallbacks(), true);
    client->setConnectTimeout(10);

    if (!client->connect(&dev, true)) {
        ESP_LOGE(TAG, "❌ Connection failed");
        NimBLEDevice::deleteClient(client);
        start_scanning();
        return;
    }

    g_client = client;
    // onConnect → secureConnection → onAuthenticationComplete → discover_hid_service
}

// ===========================================================================
// Scanning
// ===========================================================================

static void start_scanning() {
    if (g_connected) return;

    ESP_LOGI(TAG, "Starting BLE scan for Xbox controller...");

    NimBLEScan *pScan = NimBLEDevice::getScan();
    if (!pScan) {
        ESP_LOGE(TAG, "❌ Failed to get scan object");
        return;
    }

    pScan->setScanCallbacks(new XboxScanCallbacks(), true);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    pScan->start(SCAN_DURATION_MS, false);
}

// ===========================================================================
// Public API (extern "C" — callable from C code)
// ===========================================================================

extern "C" {

void xbox_ble_init(int max_devices) {
    (void)max_devices;

    // --- 1. NVS init (stores bonding keys for reconnection) ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS full or version mismatch, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // --- 2. Create mutex for thread-safe gamepad state ---
    g_mutex = xSemaphoreCreateMutex();

    // --- 3. Initialize NimBLE ---
    NimBLEDevice::init("ESP32-C6-Pet");

    // --- 4. Security: Bonding + MITM + Secure Connections (Level 4) ---
    //     Xbox Wireless Controller requires this level.
    //     setSecurityAuth(bond, mitm, sc)
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    // Just Works pairing = NoInputNoOutput on both sides + Secure Connections

    ESP_LOGI(TAG, "NimBLE initialized, security: Bonding+MITM+SC (Just Works)");

    // --- 5. Start scanning ---
    start_scanning();
}

xbox_gamepad_t xbox_ble_get_gamepad(void) {
    xbox_gamepad_t gp = {};
    if (g_mutex) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        gp = g_gamepad;
        xSemaphoreGive(g_mutex);
    }
    return gp;
}

bool xbox_ble_is_connected(void) {
    return g_connected;
}

}   // extern "C"
