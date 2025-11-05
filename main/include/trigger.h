#pragma once

#include "esp_system.h" // esp_err_t 타입을 위해 필요합니다.

/**
 * @brief Trigger/PPS 동기화 모듈 초기화
 * * 이 함수는 다음을 수행합니다:
 * 1. Trigger/PPS 핀의 GPIO 설정
 * 2. 핀 인터럽트(ISR) 서비스 설치 및 핸들러 등록
 * 3. 큐(Queue) 생성
 * 4. 시간 차이 계산을 위한 별도 태스크(timer_logic_task) 생성
 * * @return ESP_OK (성공), ESP_FAIL (실패)
 */
esp_err_t trigger_sync_init(void);