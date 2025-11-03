#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

/* ============================================================
 * GPIO 핀 매핑 (ESP32-C3 DevKit 기준)

 * ============================================================ */
#define PIN_BAT_SENSE           2   // 배터리 전압 ADC 입력
#define PIN_TRIGGER_INPUT       7   // 외부 트리거 입력 (측정 시작)
#define PIN_GPS_POWER           4   // GPS 전원 제어
#define PIN_LED                 5   
#define PIN_CHR_STATE           6   
#define PIN_UART_TX             9   // GPS UART TX
#define PIN_UART_RX             8   // GPS UART TX
#define PIN_PPS_INPUT           10   // GPS pulse per seconds

// 임시
#define PIN_DEBUG_RX            20  // 디버그 UART RX 
#define PIN_DEBUG_TX            21  // 디버그 UART TX

/* ============================================================
 * 로깅 태그
 * ============================================================ */
#define TAG_APP         "APP_MAIN"
#define TAG_ADC         "ADC_COMPONENT"
#define TAG_GPS         "GPS_COMPONENT"
#define TAG_TRIGGER     "TRIGGER"
#define TAG_MONITOR     "BOARD_INFO"
#define TAG_BATTERY     "BATTERY"
#define TAG_SYSTEM      "SYS_MANAGER"
#define TAG_COMM        "COMM_TASK"
#define TAG_BLE "BLE"

/* ============================================================
 * 유틸리티 매크로
 * ============================================================ */
#define UNUSED(x)           (void)(x)
#define GPIO_SET_LOW(pin)   gpio_set_level(pin, 0)
#define GPIO_SET_HIGH(pin)  gpio_set_level(pin, 1)

/* ============================================================
 * 시스템 상태 구조체
 * ============================================================ */
typedef struct {
    float battery_voltage;   // 현재 배터리 전압(V)
    bool battery_charging;   // 충전 중 여부
    bool rtc_time_set;       // RTC 동기화 여부
    bool gps_connected;      // GPS 모듈 연결 여부
    bool ble_connected;      // BLE 연결 여부
    bool stm_power_on;       // STM32 전원 상태
} system_status_t;

extern system_status_t system_status;

/* ============================================================
 * GPIO 초기화 함수
 * ============================================================ */
void board_gpio_init(void);


