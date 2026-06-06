idf_component_register(SRCS "buttons.c" "main.c" "display/display.c" "display/graphics.c" "display/font.c" "gamepad/controller.c" "gamepad/xbox_ble.c" "gamepad/xbox_hid.c"
                    INCLUDE_DIRS "." "display"
                    REQUIRES bt driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer nvs_flash)
