#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "common.h" 
#include "led.h"
#include "monitoring.h" 
#include "gps.h"

system_status_t system_status;

void board_gpio_init(void)
{
    ESP_LOGI(TAG_APP, "board_gpio_init() called (empty).");
}


void app_main(void)
{
    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Application Starting ---");
    memset(&system_status, 0, sizeof(system_status_t));
    board_gpio_init();

    // 3. LED 테스트 태스크 시작
    if (led_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start LED task!");
    }

    // 4. 시스템 모니터링 태스크 시작
    if (monitoring_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start Monitoring task!");
    }
    
    // 5. ⭐️ GPS 태스크 시작 ⭐️
    if (gps_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start GPS task!");
    }

    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Initialization Complete ---");
    
}