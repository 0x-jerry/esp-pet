#include "xbox_ble.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <btstack_tlv_esp32.h>

#include "btstack_config.h"
#include "btstack_hid_parser.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_defines.h"
#include "gap.h"
#include "gatt_client.h"
#include "hci_dump_embedded_stdout.h"
#include "hids_host.h"
#include "sm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

#define TAG "xbox_ble"

#define SCAN_INTERVAL_MS    30
#define SCAN_WINDOW_MS      30
#define CONNECTION_TIMEOUT_MS   10000
#define RECONNECT_DELAY_MS      200

// Xbox BLE HID report is 16 bytes + 1 report ID
#define XBOX_REPORT_SIZE    17

// HID descriptor max size (Xbox is ~283 bytes, give buffer)
#define HID_DESCRIPTOR_MAX_LEN  512

// App state machine
enum {
    STATE_IDLE,
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_PAIRING,
    STATE_HID_CONNECTING,
    STATE_READY,
};

static int g_state = STATE_IDLE;
static uint16_t g_hids_cid = 0;
static hci_con_handle_t g_con_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t g_remote_addr;
static bd_addr_type_t g_remote_addr_type;

// Gamepad state (thread-safe)
static SemaphoreHandle_t g_mutex = NULL;
static xbox_gamepad_t g_gamepad;
static bool g_connected = false;

// HID descriptor storage
static uint8_t g_hid_descriptor_storage[HID_DESCRIPTOR_MAX_LEN];

// BTstack timers & event callbacks
static btstack_timer_source_t g_connection_timer;
static btstack_timer_source_t g_reconnect_timer;
static btstack_packet_callback_registration_t g_hci_event_callback;
static btstack_packet_callback_registration_t g_sm_event_callback;

// Forward declarations
static void start_scan(void);
static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size);
static void sm_packet_handler(uint8_t packet_type, uint16_t channel,
                             uint8_t *packet, uint16_t size);
static void hids_client_packet_handler(uint8_t packet_type, uint16_t channel,
                                       uint8_t *packet, uint16_t size);

// ============================================================
// Gamepad state access (thread-safe)
// ============================================================

xbox_gamepad_t xbox_ble_get_gamepad(void) {
    xbox_gamepad_t gp;
    if (g_mutex) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        gp = g_gamepad;
        xSemaphoreGive(g_mutex);
    } else {
        memset(&gp, 0, sizeof(gp));
    }
    return gp;
}

bool xbox_ble_is_connected(void) {
    return g_connected;
}

// ============================================================
// Scanning & Connection
// ============================================================

static void start_scan(void) {
    g_state = STATE_SCANNING;
    ESP_LOGI(TAG, "BLE scanning for HID devices...");
    gap_set_scan_params(0, SCAN_INTERVAL_MS, SCAN_WINDOW_MS, 0);
    gap_start_scan();
}

static void on_connection_timeout(btstack_timer_source_t *ts) {
    UNUSED(ts);
    ESP_LOGW(TAG, "Connection timeout");
    gap_connect_cancel();
    start_scan();
}

static void connect_to_device(void) {
    ESP_LOGI(TAG, "Connecting to %s...", bd_addr_to_str(g_remote_addr));
    g_state = STATE_CONNECTING;

    btstack_run_loop_set_timer(&g_connection_timer, CONNECTION_TIMEOUT_MS);
    btstack_run_loop_set_timer_handler(&g_connection_timer, on_connection_timeout);
    btstack_run_loop_add_timer(&g_connection_timer);

    gap_stop_scan();
    gap_connect(g_remote_addr, g_remote_addr_type);
}

static void on_reconnect_timeout(btstack_timer_source_t *ts) {
    UNUSED(ts);
    g_connected = false;
    start_scan();
}

static void schedule_reconnect(void) {
    ESP_LOGI(TAG, "Scheduling reconnect...");
    g_connected = false;
    btstack_run_loop_set_timer(&g_reconnect_timer, RECONNECT_DELAY_MS);
    btstack_run_loop_set_timer_handler(&g_reconnect_timer, on_reconnect_timeout);
    btstack_run_loop_add_timer(&g_reconnect_timer);
}

// ============================================================
// HID Report Parsing (Xbox v5 BLE only)
// ============================================================

// Hat switch values to dpad bits
static uint16_t hat_to_buttons(uint8_t hat) {
    switch (hat) {
        case 1: return XB_DPAD_UP;
        case 2: return XB_DPAD_UP | XB_DPAD_RIGHT;
        case 3: return XB_DPAD_RIGHT;
        case 4: return XB_DPAD_DOWN | XB_DPAD_RIGHT;
        case 5: return XB_DPAD_DOWN;
        case 6: return XB_DPAD_DOWN | XB_DPAD_LEFT;
        case 7: return XB_DPAD_LEFT;
        case 8: return XB_DPAD_UP | XB_DPAD_LEFT;
        default: return 0;
    }
}

static void parse_hid_report(const uint8_t *report, uint16_t report_len) {
    btstack_hid_parser_t parser;
    const uint8_t *desc = hids_host_descriptor_storage_get_descriptor_data(g_hids_cid, 0);
    uint16_t desc_len = hids_host_descriptor_storage_get_descriptor_len(g_hids_cid, 0);

    if (!desc || desc_len == 0) {
        ESP_LOGW(TAG, "No HID descriptor available");
        return;
    }

    btstack_hid_parser_init(&parser, desc, desc_len,
                            HID_REPORT_TYPE_INPUT, report, report_len);

    xbox_gamepad_t gp;
    memset(&gp, 0, sizeof(gp));

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t  value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        switch (usage_page) {
            case 0x01: // Generic Desktop
                switch (usage) {
                    case 0x30: gp.axis_x = value;     break; // X (left stick X)
                    case 0x31: gp.axis_y = value;     break; // Y (left stick Y)
                    case 0x32: gp.axis_rx = value;    break; // Z (right stick X)
                    case 0x35: gp.axis_ry = value;    break; // Rz (right stick Y)
                    case 0x39: gp.dpad   = (uint8_t)value;
                               gp.buttons |= hat_to_buttons((uint8_t)value);
                               break; // Hat switch
                    default: break;
                }
                break;

            case 0x02: // Simulation Controls
                switch (usage) {
                    case 0xC5: gp.brake    = value; break; // Brake (left trigger)
                    case 0xC4: gp.throttle = value; break; // Accelerator (right trigger)
                    default: break;
                }
                break;

            case 0x09: // Button
                if (value) {
                    switch (usage) {
                        case 0x01: gp.buttons |= XB_BTN_A;      break;
                        case 0x02: gp.buttons |= XB_BTN_B;      break;
                        case 0x04: gp.buttons |= XB_BTN_X;      break;
                        case 0x05: gp.buttons |= XB_BTN_Y;      break;
                        case 0x07: gp.buttons |= XB_BTN_LB;     break;
                        case 0x08: gp.buttons |= XB_BTN_RB;     break;
                        case 0x0B: gp.buttons |= XB_BTN_SELECT; break;
                        case 0x0C: gp.buttons |= XB_BTN_START;  break;
                        case 0x0D: gp.buttons |= XB_BTN_XBOX;   break;
                        case 0x0E: gp.buttons |= XB_BTN_THUMBL; break;
                        case 0x0F: gp.buttons |= XB_BTN_THUMBR; break;
                        default: break;
                    }
                }
                break;

            case 0x0C: // Consumer
                if (usage == 0xB2 && value) {
                    gp.buttons |= XB_BTN_SHARE;
                }
                break;
        }
    }

    // Publish state
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_gamepad = gp;
    xSemaphoreGive(g_mutex);
}

// ============================================================
// HIDS Client Event Handler
// ============================================================

static void hids_client_packet_handler(uint8_t packet_type, uint16_t channel,
                                       uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED: {
            uint8_t status = gattservice_subevent_hid_service_connected_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                ESP_LOGI(TAG, "HID service connected, %d instances",
                         gattservice_subevent_hid_service_connected_get_num_instances(packet));

                // Persist bonding
                const btstack_tlv_t *tlv = btstack_tlv_esp32_get_instance();
                if (tlv) {
                    tlv->store_tag(NULL, 0x58424C31,
                                   (const uint8_t *)&g_remote_addr, sizeof(g_remote_addr));
                }

                g_state = STATE_READY;
                g_connected = true;
                ESP_LOGI(TAG, "Xbox controller ready");
            } else {
                ESP_LOGE(TAG, "HID service connection failed: %#x", status);
                gap_disconnect(g_con_handle);
            }
            break;
        }

        case GATTSERVICE_SUBEVENT_HID_REPORT: {
            const uint8_t *report = gattservice_subevent_hid_report_get_report(packet);
            uint16_t report_len = gattservice_subevent_hid_report_get_report_len(packet);
            parse_hid_report(report, report_len);
            break;
        }

        case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
            ESP_LOGW(TAG, "HID service disconnected");
            g_connected = false;
            g_state = STATE_IDLE;
            schedule_reconnect();
            break;

        default:
            break;
    }
}

// ============================================================
// SM (Security Manager) Event Handler
// ============================================================

static void sm_packet_handler(uint8_t packet_type, uint16_t channel,
                              uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_PAIRING_COMPLETE: {
            uint8_t status = sm_event_pairing_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                ESP_LOGI(TAG, "Pairing complete");
                // Connect to HID service
                g_state = STATE_HID_CONNECTING;
                hids_host_connect(g_con_handle, hids_client_packet_handler,
                                  HID_PROTOCOL_MODE_REPORT, &g_hids_cid);
            } else {
                ESP_LOGE(TAG, "Pairing failed: %#x", status);
                gap_disconnect(g_con_handle);
            }
            break;
        }

        case SM_EVENT_REENCRYPTION_COMPLETE: {
            uint8_t status = sm_event_reencryption_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                ESP_LOGI(TAG, "Re-encryption complete");
                g_state = STATE_HID_CONNECTING;
                hids_host_connect(g_con_handle, hids_client_packet_handler,
                                  HID_PROTOCOL_MODE_REPORT, &g_hids_cid);
            } else if (status == ERROR_CODE_PIN_OR_KEY_MISSING) {
                ESP_LOGI(TAG, "Bonding lost, re-pairing...");
                bd_addr_t addr;
                sm_event_reencryption_complete_get_address(packet, addr);
                gap_delete_bonding(g_remote_addr_type, addr);
                sm_request_pairing(g_con_handle);
            } else {
                ESP_LOGW(TAG, "Re-encryption failed: %#x", status);
                gap_disconnect(g_con_handle);
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================
// HCI / GAP Event Handler
// ============================================================

static bool adv_contains_hid_service(const uint8_t *packet) {
    const uint8_t *ad_data = gap_event_advertising_report_get_data(packet);
    uint8_t ad_len = gap_event_advertising_report_get_data_length(packet);

    // Search for 16-bit UUID 0x1812 (HID Service)
    for (uint8_t i = 0; i + 3 <= ad_len;) {
        uint8_t item_len = ad_data[i];
        if (item_len == 0) break;

        uint8_t item_type = ad_data[i + 1];
        if (item_type == 0x03 || item_type == 0x02) {
            // Complete/incomplete list of 16-bit UUIDs
            if (item_len >= 3) {
                uint16_t uuid = ad_data[i + 2] | (ad_data[i + 3] << 8);
                if (uuid == 0x1812) return true;
            }
        }
        i += item_len + 1;
        if (i >= ad_len) break;
    }
    return false;
}

static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE: {
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                ESP_LOGI(TAG, "BTstack ready");

                // Try to reconnect to bonded device
                const btstack_tlv_t *tlv = btstack_tlv_esp32_get_instance();
                bd_addr_t saved_addr;
                bool found = false;
                if (tlv) {
                    int len = tlv->get_tag(NULL, 0x58424C31,
                                          (uint8_t *)&saved_addr, sizeof(saved_addr));
                    found = (len == sizeof(saved_addr));
                }

                if (found) {
                    memcpy(g_remote_addr, saved_addr, 6);
                    g_remote_addr_type = BD_ADDR_TYPE_LE_RANDOM;
                    connect_to_device();
                } else {
                    start_scan();
                }
            }
            break;
        }

        case GAP_EVENT_ADVERTISING_REPORT: {
            if (g_state != STATE_SCANNING) break;
            if (!adv_contains_hid_service(packet)) return;

            gap_event_advertising_report_get_address(packet, g_remote_addr);
            g_remote_addr_type = gap_event_advertising_report_get_address_type(packet);

            int8_t rssi = gap_event_advertising_report_get_rssi(packet);
            ESP_LOGI(TAG, "Found HID device: %s (type=%u, RSSI=%d)",
                     bd_addr_to_str(g_remote_addr), g_remote_addr_type, rssi);

            connect_to_device();
            break;
        }

        case HCI_EVENT_LE_META: {
            uint8_t subevent = hci_event_le_meta_get_subevent_code(packet);
            if (subevent == HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                g_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                btstack_run_loop_remove_timer(&g_connection_timer);

                uint8_t status = hci_subevent_le_connection_complete_get_status(packet);
                if (status == ERROR_CODE_SUCCESS) {
                    ESP_LOGI(TAG, "Connected, pairing... (handle=%#x)", g_con_handle);
                    g_state = STATE_PAIRING;
                    sm_request_pairing(g_con_handle);
                } else {
                    ESP_LOGW(TAG, "Connection failed: %#x", status);
                    schedule_reconnect();
                }
            }
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            ESP_LOGW(TAG, "Disconnected");
            g_con_handle = HCI_CON_HANDLE_INVALID;
            g_hids_cid = 0;
            g_connected = false;
            g_state = STATE_IDLE;
            schedule_reconnect();
            break;
        }

        default:
            break;
    }
}

// ============================================================
// btstack_main - Entry Point
// ============================================================

int btstack_main(int argc, const char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    ESP_LOGI(TAG, "btstack_main: initializing BLE HID host");

    // Setup Security Manager (no input, no output for gamepad)
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // Init GATT client
    gatt_client_init();

    // Init HID Service Client
    hids_host_init(g_hid_descriptor_storage, sizeof(g_hid_descriptor_storage));

    // Register HCI event handler
    g_hci_event_callback.callback = &packet_handler;
    hci_add_event_handler(&g_hci_event_callback);

    // Register SM event handler
    g_sm_event_callback.callback = &sm_packet_handler;
    sm_add_event_handler(&g_sm_event_callback);

    // Power on
    hci_power_control(HCI_POWER_ON);

    return 0;
}

// ============================================================
// Public: xbox_ble_init - Called from app_main
// ============================================================

void xbox_ble_init(int max_devices) {
    UNUSED(max_devices);

    // Create mutex for gamepad state
    g_mutex = xSemaphoreCreateMutex();

#ifdef CONFIG_ESP_CONSOLE_UART
    btstack_stdio_init();
#endif

    btstack_init();
    btstack_main(0, NULL);
    btstack_run_loop_execute();
}
