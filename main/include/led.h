#pragma once
#include "esp_err.h"

/**
 * @brief LED 테스트 태스크를 생성하고 초기화합니다.
 * * 이 함수는 LED 테스트(점멸, SOS, PWM)를 무한 반복하는
 * 별도의 FreeRTOS 태스크를 생성합니다.
 * * @return esp_err_t 
 * - ESP_OK: 태스크 생성 성공
 * - ESP_FAIL: 태스크 생성 실패
 */
esp_err_t led_task_init(void);