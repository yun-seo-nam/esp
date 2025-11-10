#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h" // ⭐️ ESP_LOG* 매크로를 위해 추가

#include "common.h" 
#include "led.h"
#include "monitoring.h" 
#include "gps.h"
#include "trigger.h"    
#include "ble_nimble.h" // ⭐️ 'ble_nimble.h'

// ⭐️ 네가 준 'gap.c' 예제 코드에 이게 있었어.
// ⭐️ 'common.h'에 'system_status_t'가 정의되어 있어야 해.
system_status_t system_status; 

void app_main(void)
{
    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Application Starting ---");
    memset(&system_status, 0, sizeof(system_status_t));

    // LED 태스크 시작
    if (led_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start LED task!");
    }

    // ⭐️ BLE 초기화 (다른 태스크보다 먼저)
    if (ble_nimble_stack_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to initialize NimBLE stack!");
    }

    if (trigger_sync_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start Trigger Sync test module!");
    }

    if (gps_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start GPS task!");
    }
    
    // 4. 시스템 모니터링 태스크 시작
    if (monitoring_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start Monitoring task!");
    }

    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Initialization Complete ---");
}