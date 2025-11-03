#pragma once
#include "esp_err.h"

/**
 * @brief 시스템 모니터링 태스크를 생성하고 시작합니다.
 *
 * 이 태스크는 낮은 우선순위로 주기적으로 실행되며,
 * 다른 태스크의 스택 사용량(HWM)과 전체 힙 메모리 잔량을 로깅합니다.
 *
 * @return esp_err_t
 * - ESP_OK: 태스크 생성 성공
 * - ESP_FAIL: 태스크 생성 실패
 */
esp_err_t monitoring_task_init(void);