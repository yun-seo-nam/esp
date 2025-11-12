#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "common.h"
#include "led.h"
#include "monitoring.h"
#include "gps.h"
#include "trigger.h"
#include "ble.h"

system_status_t system_status;

void app_main(void)
{
    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Application Starting ---");
    memset(&system_status, 0, sizeof(system_status_t));

    // LED 태스크 시작
    if (led_task_init() != ESP_OK)
    {
        ESP_LOGE(TAG_APP, "Failed to start LED task!");
    }

    //if (ble_nimble_init() != ESP_OK) {
    //    ESP_LOGE("APP", "BLE init failed");
    //    return;
    //}

    //ble_set_offset_value(987654);
    //ble_set_gps_value("37.5665,126.9780,2025-11-12T09:00:00Z");

    //if (trigger_sync_init() != ESP_OK)
    //{
    //    ESP_LOGE(TAG_APP, "Failed to start Trigger Sync test module!");
    //}

    if (gps_task_init() != ESP_OK)
    {
        ESP_LOGE(TAG_APP, "Failed to start GPS task!");
    }

    // 4. 시스템 모니터링 태스크 시작
    //if (monitoring_task_init() != ESP_OK)
    //{
    //    ESP_LOGE(TAG_APP, "Failed to start Monitoring task!");
    //}

    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Initialization Complete ---");
}