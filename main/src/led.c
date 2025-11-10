#include "led.h"       
#include "common.h"     
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

/**
 * @brief LED 테스트를 무한 반복하는 FreeRTOS 태스크
 */
static void led_test_task(void *arg)
{
    ESP_LOGI(TAG_APP, "LED Test Task started.");

    configure_led_gpio(); 
    ESP_LOGI(TAG_APP, "GPIO configured. Starting blink loop.");

    while (1) 
    {
        GPIO_SET_HIGH(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
        GPIO_SET_LOW(PIN_LED);
        vTaskDelay(pdMS_TO_TICKS(500));
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
        led_test_task,          
        "led_test_task",    
        2048,               
        NULL,                 
        5,                   
        &xLedTaskHandle        
    );

    if (res == pdPASS) {
        ESP_LOGI(TAG_APP, "LED Test Task created successfully.");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_APP, "Failed to create LED Test Task.");
        return ESP_FAIL;
    }
}