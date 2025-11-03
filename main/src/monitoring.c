#include "monitoring.h" // 자신의 헤더 파일
#include "common.h"     // 공통 헤더 (TAG_MONITOR 등)
#include "gps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h" // esp_get_free_heap_size 용

/* ============================================================
 * 설정값
 * ============================================================ */
#define MONITORING_TASK_STACK_SIZE  2048 // 로그 출력을 하므로 넉넉하게
#define MONITORING_TASK_PRIORITY    1    // 가장 낮은 우선순위
#define MONITORING_INTERVAL_MS      10000 // 10초

/* ============================================================
 * 외부 태스크 핸들 참조
 * ============================================================ */
// led.c 에서 정의된 핸들을 '참조'하겠다고 선언 (extern)
extern TaskHandle_t xLedTaskHandle;

/* ============================================================
 * Private 함수
 * ============================================================ */

/**
 * @brief 시스템 상태(스택 HWM, 힙)를 주기적으로 모니터링하는 태스크
 */
static void system_monitoring_task(void *arg)
{
    ESP_LOGI(TAG_MONITOR, "System Monitoring Task started.");

    // 이 태스크의 무한 루프
    while (1)
    {
        // MONITORING_INTERVAL_MS (10초) 마다 한 번씩 실행
        vTaskDelay(pdMS_TO_TICKS(MONITORING_INTERVAL_MS)); 

        ESP_LOGI(TAG_MONITOR, "--- [System Monitor] ---");

        // 1. LED 태스크 스택 HWM 확인
        if (xLedTaskHandle != NULL)
        {
            // 태스크가 사용하고 '남은' 최소 스택 공간(HWM)
            UBaseType_t hwm_words = uxTaskGetStackHighWaterMark(xLedTaskHandle);
            uint32_t hwm_bytes = hwm_words * sizeof(StackType_t); 

            ESP_LOGI(TAG_MONITOR, "LED Task Stack HWM (남은 공간): %u Bytes", hwm_bytes);
        }

        // 2. GPS 태스크 스택 HWM 확인
        if (xGpsTaskHandle != NULL)
        {
            // 태스크가 사용하고 '남은' 최소 스택 공간(HWM)
            UBaseType_t hwm_words = uxTaskGetStackHighWaterMark(xGpsTaskHandle);
            uint32_t hwm_bytes = hwm_words * sizeof(StackType_t); 

            ESP_LOGI(TAG_MONITOR, "GPS Task Stack HWM (남은 공간): %u Bytes", hwm_bytes);
        }
        
        // 3. 전체 힙(Heap) 메모리 잔량 확인
        uint32_t free_heap = esp_get_free_heap_size();
        ESP_LOGI(TAG_MONITOR, "Free Heap Size: %u Bytes", free_heap);

        // 4. 이 모니터링 태스크 자신의 HWM 확인 (NULL = 자기 자신)
        UBaseType_t self_hwm_words = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG_MONITOR, "Monitor Task HWM (남은 공간): %u Bytes", (self_hwm_words * sizeof(StackType_t)));

        ESP_LOGI(TAG_MONITOR, "--------------------------");
    }
}

/* ============================================================
 * Public 함수 (monitoring.h 에 선언됨)
 * ============================================================ */

/**
 * @brief 모니터링 태스크를 생성하고 초기화합니다.
 */
esp_err_t monitoring_task_init(void)
{
    BaseType_t res = xTaskCreate(
        system_monitoring_task, // 실행할 함수
        "monitor_task",         // 태스크 이름
        MONITORING_TASK_STACK_SIZE,
        NULL,                   // 파라미터 없음
        MONITORING_TASK_PRIORITY,
        NULL                    // 핸들 필요 없음
    );

    if (res == pdPASS) {
        ESP_LOGI(TAG_APP, "Monitoring Task created successfully.");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_APP, "Failed to create Monitoring Task.");
        return ESP_FAIL;
    }
}