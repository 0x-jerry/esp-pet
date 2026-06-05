set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../external/btstack)

set(include_dirs
    "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include"
    "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include"
    "${BTSTACK_ROOT}/3rd-party/hxcmod-player"
    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods"
    "${BTSTACK_ROOT}/3rd-party/lc3-google/include"
    "${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server"
    "${BTSTACK_ROOT}/3rd-party/md5"
    "${BTSTACK_ROOT}/3rd-party/yxml"
    "${BTSTACK_ROOT}/src/classic"
    "${BTSTACK_ROOT}/src/ble/gatt-service"
    "${BTSTACK_ROOT}/src/ble"
    "${BTSTACK_ROOT}/src"
    "${BTSTACK_ROOT}/platform/embedded"
    "${BTSTACK_ROOT}/platform/freertos"
    "${BTSTACK_ROOT}/platform/lwip"
    "${BTSTACK_ROOT}/port/esp32/components/btstack/include"
    "${BTSTACK_ROOT}/chipset/zephyr"
    "${IDF_PATH}/components/lwip/lwip/src/include"
    "include"
)

set(src_dirs
    "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce"
    "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce"
    "${BTSTACK_ROOT}/3rd-party/hxcmod-player"
    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods"
    "${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server"
    "${BTSTACK_ROOT}/3rd-party/md5"
    "${BTSTACK_ROOT}/3rd-party/micro-ecc"
    "${BTSTACK_ROOT}/3rd-party/yxml"
    "${BTSTACK_ROOT}/src/ble/gatt-service"
    "${BTSTACK_ROOT}/src/ble"
    "${BTSTACK_ROOT}/src"
    "${BTSTACK_ROOT}/platform/embedded"
    "${BTSTACK_ROOT}/platform/freertos"
    "${BTSTACK_ROOT}/platform/lwip"
    "${BTSTACK_ROOT}/port/esp32/components/btstack"
    "${BTSTACK_ROOT}/chipset/zephyr"
)

set(exclude_srcs
    "${BTSTACK_ROOT}/port/esp32/components/btstack/btstack_audio_esp32_v4.c"
    "${BTSTACK_ROOT}/port/esp32/components/btstack/btstack_audio_esp32_v5.c"
    "${BTSTACK_ROOT}/port/esp32/components/btstack/es8388.c"
    "${BTSTACK_ROOT}/port/esp32/components/btstack/hal_uart_dma_esp32.c"
    "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c"
)

set(priv_include_dirs
    "${BTSTACK_ROOT}/3rd-party/micro-ecc"
)

set(priv_requires
    "nvs_flash"
    "bt"
    "driver"
    "esp_driver_gpio"
    "esp_driver_uart"
    "esp_driver_usb_serial_jtag"
    "lwip"
    "vfs"
)

idf_component_register(SRC_DIRS "${src_dirs}"
                    EXCLUDE_SRCS "${exclude_srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS "${priv_include_dirs}"
                    PRIV_REQUIRES ${priv_requires})
