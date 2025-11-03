file(GLOB_RECURSE srcs "src/*.c")

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio
)
