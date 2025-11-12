#pragma once
#include "esp_err.h"

esp_err_t ble_nimble_init(void);

// pps offset
void ble_set_offset_value(int64_t v);

void ble_set_gps_value(const char *gps);
