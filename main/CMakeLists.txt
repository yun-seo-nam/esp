file(GLOB_RECURSE srcs "src/*.c")

set(EXTRA_COMPONENT_DIRS
    ${IDF_PATH}/components/bt/host/nimble/nimble/porting/nimble/include
)
include_directories(${EXTRA_COMPONENT_DIRS})

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart esp_driver_gpio esp_timer nvs_flash bt
)
