#pragma once

#include "esp_err.h"
#include <stdint.h> 

/**
 * @brief NimBLE 스택 전체(NVS, 컨트롤러, GAP, GATT)를 초기화하고 태스크를 시작합니다.
 */
esp_err_t ble_nimble_stack_init(void);

/**
 * @brief (데이터 공유용) 다른 C 파일에서 Offset 값을 가져올 수 있게 하는 함수
 */
void ble_gatt_set_offset_value(int64_t offset_us);

/**
 * @brief (데이터 공유용) 다른 C 파일에서 GPS 값을 가져올 수 있게 하는 함수
 */
void ble_gatt_set_gps_value(const char* gps_str);