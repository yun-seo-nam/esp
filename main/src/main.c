#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "common.h" 
#include "led.h"
#include "monitoring.h" 
#include "gps.h"
#include "trigger.h"    // <-- 1. 새로 추가한 헤더 파일

// 이 값을 0으로 바꾸면 Trigger/PPS 테스트가 비활성화됩니다.
#define ENABLE_TRIGGER_SYNC_TEST 1

// (⭐️ 여기 있던 'PPS/Trigger 동기화 테스트 모듈' 코드는 전부 trigger.c로 이동했습니다 ⭐️)

// --- 기존 코드 ---
system_status_t system_status;

void board_gpio_init(void)
{
    ESP_LOGI(TAG_APP, "board_gpio_init() called.");

    // (⭐️ Trigger/PPS 핀 설정이 trigger.c로 이동했으므로 여기서 삭제 ⭐️)

    // (여기에 LED, GPS 전원 핀 등 다른 GPIO 설정만 남겨둡니다)
}


void app_main(void)
{
    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Application Starting ---");
    memset(&system_status, 0, sizeof(system_status_t));
    
    // 1. 공용 GPIO 초기화 (Trigger/PPS 핀 설정은 trigger_sync_init()이 알아서 함)
    board_gpio_init();

    // 2. LED 태스크 시작
    if (led_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start LED task!");
    }

#if ENABLE_TRIGGER_SYNC_TEST
    // 3. [TEST] Trigger/PPS 시간 차이 측정 태스크 시작
    if (trigger_sync_init() != ESP_OK) { // <-- 2. 그냥 이 함수만 호출하면 끝
        ESP_LOGE(TAG_APP, "Failed to start Trigger Sync test module!");
    }
#endif

    // 4. 시스템 모니터링 태스크 시작
    // if (monitoring_task_init() != ESP_OK) {
    //     ESP_LOGE(TAG_APP, "Failed to start Monitoring task!");
    // }
    
    // 5. GPS 태스크 시작 
    if (gps_task_init() != ESP_OK) {
        ESP_LOGE(TAG_APP, "Failed to start GPS task!");
    }

    ESP_LOGI(TAG_APP, "--- [APP_MAIN] Initialization Complete ---");
}