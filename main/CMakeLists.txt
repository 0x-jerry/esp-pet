idf_component_register(SRCS "buttons.c" "main.c" "display.c" "font.c" "controller.c" "xbox_ble.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp-nimble-cpp driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer)
