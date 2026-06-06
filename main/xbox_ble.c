/**
 * xbox_ble.c — ESP-IDF Native NimBLE Xbox Wireless Controller HID host
 *
 * Flow: on_sync → start_scan → DISC event → connect → ENC_CHANGE →
 *        discover HID service → discover characteristics →
 *        discover CCCD → subscribe → NOTIFY_RX (gamepad data)
 */
#include "xbox_ble.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ── NimBLE native headers ─────────────────────────────────────────── */
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#define TAG "xbox_ble"

/* ── BLE UUIDs (16-bit) ────────────────────────────────────────────── */
#define HID_SVC_UUID        0x1812
#define BATTERY_SVC_UUID    0x180F
#define HID_REPORT_UUID     0x2A4D
#define HID_REPORT_MAP_UUID 0x2A4B
#define HID_INFO_UUID       0x2A4A
#define CCCD_UUID           0x2902
#define BATTERY_LEVEL_UUID  0x2A19

/* ── Timing ────────────────────────────────────────────────────────── */
#define SCAN_DURATION_MS   5000
#define RECONNECT_DELAY_MS 500
#define CONNECT_TIMEOUT_MS 8000

/* ── Xbox input report minimum length ───────────────────────────────── */
#define XBOX_REPORT_MIN_LEN 17

/* ── Globals ───────────────────────────────────────────────────────── */
static SemaphoreHandle_t g_mutex       = NULL;
static xbox_gamepad_t    g_gamepad     = {0};
static bool              g_connected   = false;
static uint16_t          g_conn_handle = BLE_HS_CONN_HANDLE_NONE;

/* HID service handle range */
static uint16_t g_hid_start_handle = 0;
static uint16_t g_hid_end_handle   = 0;

/* ── Forward declarations ──────────────────────────────────────────── */
static int  gap_event_cb(struct ble_gap_event *event, void *arg);
static void start_scan(void);
static int  service_discovery_cb(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *svc, void *arg);
static int  char_discovery_cb(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              const struct ble_gatt_chr *chr, void *arg);
static int  desc_discovery_cb(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              uint16_t chr_val_handle,
                              const struct ble_gatt_dsc *dsc, void *arg);
static int  write_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     struct ble_gatt_attr *attr, void *arg);

/* =====================================================================
 *    GAP Event Callback — all BLE events handled here
 * ===================================================================== */
static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {

    /* ── Advertisement report ──────────────────────────────────── */
    case BLE_GAP_EVENT_DISC: {
        if (g_connected) return 0;

        struct ble_hs_adv_fields fields;
        int rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                         event->disc.length_data);
        if (rc != 0) return 0;

        /* Read device name from advertisement */
        char name[32] = {0};
        if (fields.name_len > 0 && fields.name_len < sizeof(name)) {
            memcpy(name, fields.name, fields.name_len);
        }

        /* Only process Xbox controllers */
        if (strstr(name, "Xbox") == NULL && strstr(name, "xbox") == NULL) {
            return 0;
        }

        char addr_str[18];
        sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                event->disc.addr.val[5], event->disc.addr.val[4],
                event->disc.addr.val[3], event->disc.addr.val[2],
                event->disc.addr.val[1], event->disc.addr.val[0]);

        ESP_LOGI(TAG, "🎮 Found Xbox: %s [%s] RSSI=%d",
                 name, addr_str, event->disc.rssi);

        /* Stop scan, then connect */
        ble_gap_disc_cancel();
        g_connected = false; /* allow new connection attempt */

        int rc2 = ble_gap_connect(BLE_OWN_ADDR_PUBLIC,
                                  &event->disc.addr,
                                  CONNECT_TIMEOUT_MS,
                                  NULL,
                                  gap_event_cb,
                                  NULL);
        if (rc2 != 0) {
            ESP_LOGE(TAG, "❌ ble_gap_connect failed: %d", rc2);
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            start_scan();
        }
        return 0;
    }

    /* ── Connection established ─────────────────────────────────── */
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status != 0) {
            ESP_LOGE(TAG, "❌ Connect failed: status=%d",
                     event->connect.status);
            g_connected = false;
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            start_scan();
            return 0;
        }

        g_conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "✅ Connected (handle=%d)", g_conn_handle);

        /* Initiate pairing / encryption immediately */
        int rc = ble_gap_security_initiate(g_conn_handle);
        if (rc != 0) {
            ESP_LOGE(TAG, "❌ Security initiate failed: %d", rc);
        } else {
            ESP_LOGI(TAG, "🔐 Pairing initiated...");
        }
        return 0;
    }

    /* ── Disconnect ─────────────────────────────────────────────── */
    case BLE_GAP_EVENT_DISCONNECT: {
        ESP_LOGI(TAG, "Disconnected (reason=%d)",
                 event->disconnect.reason);
        g_connected   = false;
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        g_hid_start_handle = 0;
        g_hid_end_handle   = 0;

        /* Reconnect */
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        start_scan();
        return 0;
    }

    /* ── Encryption state change (pairing complete/fail) ────────── */
    case BLE_GAP_EVENT_ENC_CHANGE: {
        if (event->enc_change.status != 0) {
            ESP_LOGE(TAG, "❌ Encryption failed: status=%d",
                     event->enc_change.status);
            ble_gap_terminate(g_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }

        ESP_LOGI(TAG, "🔐 Link encrypted! Discovering services...");

        /* Discover all services */
        int rc = ble_gattc_disc_all_svcs(g_conn_handle,
                                         service_discovery_cb, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "❌ Service discovery error: %d", rc);
        }
        return 0;
    }

    /* ── Notify / Indicate data received (gamepad input reports) ── */
    case BLE_GAP_EVENT_NOTIFY_RX: {
        struct os_mbuf *om = event->notify_rx.om;
        uint16_t len = OS_MBUF_PKTLEN(om);

        if (len >= XBOX_REPORT_MIN_LEN) {
            uint8_t data[XBOX_REPORT_MIN_LEN];
            os_mbuf_copydata(om, 0, len, data);

            if (data[0] == 0x01) {
                /* Main input report */
                xbox_gamepad_t gp = {0};

                gp.axis_x   = (int32_t)(data[1]  | (data[2]  << 8));
                gp.axis_y   = (int32_t)(data[3]  | (data[4]  << 8));
                gp.axis_rx  = (int32_t)(data[5]  | (data[6]  << 8));
                gp.axis_ry  = (int32_t)(data[7]  | (data[8]  << 8));
                gp.brake    = (int32_t)((data[9]  | (data[10] << 8)) & 0x03FF);
                gp.throttle = (int32_t)((data[11] | (data[12] << 8)) & 0x03FF);
                gp.dpad     = data[13] & 0x0F;
                gp.buttons  = (uint16_t)(data[14] | (data[15] << 8));

                if (g_mutex) {
                    xSemaphoreTake(g_mutex, portMAX_DELAY);
                    g_gamepad = gp;
                    xSemaphoreGive(g_mutex);
                }
            } else if (data[0] == 0x04) {
                /* Battery report */
                ESP_LOGI(TAG, "🔋 Battery: %d%%", data[1]);
            }
        }
        return 0;
    }

    /* ── MTU change ─────────────────────────────────────────────── */
    case BLE_GAP_EVENT_MTU: {
        ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
        return 0;
    }

    default:
        return 0;
    }
}

/* =====================================================================
 *    Service Discovery Callback
 * ===================================================================== */
static int service_discovery_cb(uint16_t conn_handle,
                                const struct ble_gatt_error *error,
                                const struct ble_gatt_svc *svc,
                                void *arg) {
    (void)conn_handle;
    (void)arg;


    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "❌ Service discovery error: %d", error->status);
        return 0;
    }

    /* svc == NULL means discovery complete */
    if (svc == NULL) {
        ESP_LOGI(TAG, "✅ Service discovery complete");

        if (g_hid_start_handle == 0) {
            ESP_LOGE(TAG, "❌ HID service not found!");
            ble_gap_terminate(g_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }

        ESP_LOGI(TAG, "Discovering HID characteristics [%d..%d]...",
                 g_hid_start_handle, g_hid_end_handle);

        ble_gattc_disc_all_chrs(g_conn_handle,
                                g_hid_start_handle, g_hid_end_handle,
                                char_discovery_cb, NULL);
        return 0;
    }


    /* Print each discovered service */
    char uuid_str[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&svc->uuid.u, uuid_str);
    ESP_LOGI(TAG, "Service: %s [%d..%d]", uuid_str,
             svc->start_handle, svc->end_handle);

    /* Record HID service handle range */
    if (svc->uuid.u.type == BLE_UUID_TYPE_16 &&
        svc->uuid.u16.value == HID_SVC_UUID) {
        g_hid_start_handle = svc->start_handle;
        g_hid_end_handle   = svc->end_handle;
    }

    return 0;
}

/* =====================================================================
 *    Characteristic Discovery Callback
 * ===================================================================== */
static int char_discovery_cb(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             const struct ble_gatt_chr *chr,
                             void *arg) {
    (void)conn_handle;
    (void)arg;

    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "❌ Char discovery error: %d", error->status);
        return 0;
    }

    if (chr == NULL) {
        ESP_LOGI(TAG, "✅ Characteristic discovery complete");
        return 0;
    }

    char uuid_str[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&chr->uuid.u, uuid_str);

    ESP_LOGI(TAG, "  Char: %s handle=%d props=0x%02X",
             uuid_str, chr->val_handle, chr->properties);

    /* If this is the HID Report char with Notify support → discover CCCD */
    if (chr->uuid.u.type == BLE_UUID_TYPE_16 &&
        chr->uuid.u16.value == HID_REPORT_UUID &&
        (chr->properties & BLE_GATT_CHR_PROP_NOTIFY)) {

        ESP_LOGI(TAG, "Found HID Report, discovering descriptors...");
        ble_gattc_disc_all_dscs(g_conn_handle,
                                chr->val_handle, g_hid_end_handle,
                                desc_discovery_cb, NULL);
    }

    return 0;
}

/* =====================================================================
 *    Descriptor Discovery + CCCD Subscribe Callback
 * ===================================================================== */
static int desc_discovery_cb(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             uint16_t chr_val_handle,
                             const struct ble_gatt_dsc *dsc,
                             void *arg) {
    (void)conn_handle;
    (void)chr_val_handle;
    (void)arg;

    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "❌ Descriptor discovery error: %d", error->status);
        return 0;
    }

    if (dsc == NULL) return 0;

    char uuid_str[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&dsc->uuid.u, uuid_str);
    ESP_LOGI(TAG, "    Desc: %s handle=%d", uuid_str, dsc->handle);

    /* Found CCCD → enable Notifications */
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16 &&
        dsc->uuid.u16.value == CCCD_UUID) {

        uint16_t enable = 0x0001;  /* Enable notifications */
        int rc = ble_gattc_write_flat(g_conn_handle, dsc->handle,
                                      &enable, sizeof(enable),
                                      write_cb, NULL);
        if (rc == 0) {
            ESP_LOGI(TAG, "CCCD write sent (enable notify)");
        } else {
            ESP_LOGE(TAG, "❌ CCCD write failed: %d", rc);
        }
    }

    return 0;
}

/* =====================================================================
 *    Write Complete Callback
 * ===================================================================== */
static int write_cb(uint16_t conn_handle,
                    const struct ble_gatt_error *error,
                    struct ble_gatt_attr *attr,
                    void *arg) {
    (void)attr;
    (void)arg;
    if (error->status == 0) {
        ESP_LOGI(TAG, "✅ Subscribed to HID Report!");
        g_connected = true;
    } else {
        ESP_LOGE(TAG, "❌ Subscribe failed: %d", error->status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

/* =====================================================================
 *    Scanning
 * ===================================================================== */
static void start_scan(void) {
    if (g_connected) return;

    ESP_LOGI(TAG, "Scanning for Xbox controller...");

    struct ble_gap_disc_params params = {
        .itvl              = 0x0010,   /* 16 * 0.625ms = 10ms */
        .window            = 0x0010,
        .filter_policy     = 0,
        .limited           = 0,
        .passive           = 0,        /* active scan */
        .filter_duplicates = 0,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC,
                          SCAN_DURATION_MS * 1000 / 625,  /* ms → ticks */
                          &params,
                          gap_event_cb,
                          NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "❌ Scan start failed: %d, retrying...", rc);
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        start_scan();
    }
}

/* =====================================================================
 *    Public API — callable from main.c / controller.c
 * ===================================================================== */

void xbox_ble_init(int max_devices) {
    (void)max_devices;

    /* 1. Create mutex for thread-safe gamepad state */
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create mutex");
        return;
    }

    /* 2. Reset state */
    g_connected        = false;
    g_conn_handle      = BLE_HS_CONN_HANDLE_NONE;
    g_hid_start_handle = 0;
    g_hid_end_handle   = 0;

    ESP_LOGI(TAG, "NimBLE HID host ready (Bonding+SC+JustWorks)");

    /* 3. Start scanning (runs in NimBLE host task context) */
    start_scan();
}

xbox_gamepad_t xbox_ble_get_gamepad(void) {
    xbox_gamepad_t gp = {0};
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
