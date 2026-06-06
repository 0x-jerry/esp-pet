idf_component_register(SRCS "buttons.c" "main.c" "display.c" "font.c" "controller.c" "xbox/xbox_ble.c" "xbox/xbox_hid.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer nvs_flash)
