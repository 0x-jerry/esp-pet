# 1. Gather all directories containing source files
set(my_src_dirs 
    "." 
    "pet" 
    "ui" 
    "display" 
    "gamepad"
)

# 2. Register the component using SRC_DIRS instead of SRCS
idf_component_register(
    SRC_DIRS ${my_src_dirs}
    INCLUDE_DIRS ${my_src_dirs}
    REQUIRES bt driver esp_lcd esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer nvs_flash
)
