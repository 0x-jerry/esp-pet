idf_component_register(SRCS "buttons.c" "main.c" "pet/pet_state.c" "pet/pet_sprite.c" "ui/ui.c" "ui/ui_state.c" "display/display.c" "display/gfx_init.c" "display/gfx_helper.c" "gamepad/controller.c" "gamepad/xbox_ble.c" "gamepad/xbox_hid.c"
                    INCLUDE_DIRS "." "display" "pet" "ui"
                    REQUIRES bt driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer nvs_flash)
