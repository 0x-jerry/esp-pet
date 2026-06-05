idf_component_register(SRCS "buttons.c" "main.c" "display.c" "font.c" "controller.c" "my_platform.c"
                    INCLUDE_DIRS "."
                    REQUIRES bluepad32 btstack driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer)
