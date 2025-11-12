file(GLOB_RECURSE srcs "src/*.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash bt esp_timer esp_driver_gpio esp_driver_uart
)