// main/led.c

#include "led.h"        // 방금 만든 헤더파일
#include "common.h"     // 공통 헤더
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

TaskHandle_t xLedTaskHandle = NULL;

static void configure_led_gpio(void)
{
    ESP_LOGD(TAG_APP, "Configuring GPIO%d as output", PIN_LED);
    gpio_reset_pin(PIN_LED); // 핀 리셋
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT); // 출력 모드
}

static void test_simple_blink(void)
{
    ESP_LOGI(TAG_APP, "Starting simple blink test (5 times)");
        GPIO_SET_HIGH(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
        GPIO_SET_LOW(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
}

/**
 * @brief LED 테스트를 무한 반복하는 FreeRTOS 태스크
 */
static void led_test_task(void *arg)
{
    ESP_LOGI(TAG_APP, "LED Test Task started.");

    // 1. GPIO 설정을 while(1) 루프 밖에서 "한 번만" 실행합니다.
    configure_led_gpio(); 
    ESP_LOGI(TAG_APP, "GPIO configured. Starting blink loop.");

    // 2. 이 태스크의 무한 루프
    while (1) 
    {
        // test_simple_blink() 함수의 내용을 직접 넣습니다.
        GPIO_SET_HIGH(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
        GPIO_SET_LOW(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // 불필요한 추가 딜레이(vTaskDelay(pdMS_TO_TICKS(500)))를 제거했습니다.
        // 불필요한 ESP_LOGI("--- Running GPIO Tests ---")도 제거했습니다.
    }
}

/* ============================================================
 * 전역 함수 (Global Functions)
 * led.h에 선언된 함수입니다.
 * ============================================================ */

/**
 * @brief LED 테스트 태스크를 생성하고 초기화합니다.
 */
esp_err_t led_task_init(void)
{
    // xTaskCreate( [함수 포인터], [태스크 이름], [스택 크기], [파라미터], [우선순위], [핸들] )
    BaseType_t res = xTaskCreate(
        led_test_task,          // 이 태스크가 실행할 함수
        "led_test_task",        // 태스크 이름 (디버깅용)
        2048,                   // 스택 크기 (Bytes) - 넉넉하게
        NULL,                   // 태스크 함수에 전달할 파라미터 (없음)
        5,                      // 태스크 우선순위 (중간)
        &xLedTaskHandle         // 태스크 핸들 (필요 없음)
    );

    if (res == pdPASS) {
        ESP_LOGI(TAG_APP, "LED Test Task created successfully.");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_APP, "Failed to create LED Test Task.");
        return ESP_FAIL;
    }
}