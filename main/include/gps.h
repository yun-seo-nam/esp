#ifndef GPS_H
#define GPS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// 외부 접근용 선언
extern TaskHandle_t xGpsTaskHandle;

esp_err_t gps_task_init(void);

#endif 
