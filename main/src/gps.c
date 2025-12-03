#include "gps.h"        
#include "ble.h"
#include "common.h"     

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h" 
#include "string.h"      
#include "stdbool.h"     

#include <stdio.h>      
#include <math.h>       
#include <time.h>       
#include <sys/time.h>   
#include <stdlib.h>     

/* ============================================================
 * GPS UART 설정값
 * ============================================================ */
#define GPS_UART_NUM        UART_NUM_1 
#define GPS_UART_BAUD_RATE  9600
#define GPS_TASK_STACK_SIZE 4096 
#define GPS_UART_BUF_SIZE   1024 

TaskHandle_t xGpsTaskHandle = NULL;

/* ============================================================
 * GPS 데이터 버퍼 (1초 배치용)
 * ============================================================ */
static char g_gga_line[128] = {0};
static char g_rmc_line[128] = {0};

static bool g_has_gga = false;
static bool g_has_rmc = false;

/* ============================================================
 * forward declarations
 * ============================================================ */
static bool print_gps_info(char *gga, char *rmc);
static double nmea_to_decimal(double nmea_val);
static void set_korea_timezone(void);

/* ============================================================
 * UART 초기화
 * ============================================================ */
static esp_err_t init_gps_uart(void)
{
    ESP_LOGI(TAG_GPS, "Configuring GPS Power Pin (GPIO%d) to HIGH", PIN_GPS_POWER);
    gpio_reset_pin(PIN_GPS_POWER);
    gpio_set_direction(PIN_GPS_POWER, GPIO_MODE_OUTPUT);
    GPIO_SET_HIGH(PIN_GPS_POWER);

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG_GPS, "Initializing UART%d for GPS module...", GPS_UART_NUM);

    uart_config_t uart_config = {
        .baud_rate = GPS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(GPS_UART_NUM,
                                        GPS_UART_BUF_SIZE * 2,
                                        0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_GPS, "Failed to install UART driver: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(GPS_UART_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_GPS, "Failed to set UART parameters: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(GPS_UART_NUM,
                       PIN_UART_TX,
                       PIN_UART_RX,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_GPS, "Failed to set UART pins: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG_GPS, "UART%d initialized successfully (RX:%d, TX:%d)",
             GPS_UART_NUM, PIN_UART_RX, PIN_UART_TX);

    return ESP_OK;
}

/* ============================================================
 * NMEA 문장 파싱
 * ============================================================ */
static void parse_nmea_sentence(const char *line)
{
    ESP_LOGD(TAG_GPS, "Raw: %s", line);

    if (strstr(line, "$GNGGA") || strstr(line, "$GPGGA")) {
        strncpy(g_gga_line, line, sizeof(g_gga_line) - 1);
        g_gga_line[sizeof(g_gga_line) - 1] = '\0';
        g_has_gga = true;
    }
    else if (strstr(line, "$GNRMC") || strstr(line, "$GPRMC")) {
        strncpy(g_rmc_line, line, sizeof(g_rmc_line) - 1);
        g_rmc_line[sizeof(g_rmc_line) - 1] = '\0';
        g_has_rmc = true;
    }

    if (g_has_gga && g_has_rmc)
    {
        print_gps_info(g_gga_line, g_rmc_line);

        g_has_gga = false;
        g_has_rmc = false;

        g_gga_line[0] = '\0';
        g_rmc_line[0] = '\0';
    }
}

/* ============================================================
 * NMEA → 소수점 좌표 변환
 * ============================================================ */
static double nmea_to_decimal(double nmea_val)
{
    int degrees = (int)(nmea_val / 100);
    double minutes = nmea_val - (degrees * 100);
    return degrees + (minutes / 60.0);
}

/* ============================================================
 * GPS 데이터 처리 (GGA + RMC)
 * ============================================================ */
static bool print_gps_info(char *gga, char *rmc)
{
    char *saveptr_rmc, *saveptr_gga;
    char *token;

    /* ---------- RMC 파싱 ---------- */
    char *rmc_fields[14] = {0};
    int idx = 0;

    token = strtok_r(rmc, ",", &saveptr_rmc);
    while (token != NULL && idx < 14) {
        rmc_fields[idx++] = token;
        token = strtok_r(NULL, ",", &saveptr_rmc);
    }

    char status = (rmc_fields[2]) ? rmc_fields[2][0] : 'V';

    if (status == 'V') {
        ESP_LOGI(TAG_GPS, "[NO FIX] Waiting for satellites...");
        ble_set_gps_value("[NO FIX] Waiting...");
        return false;
    }

    double rmc_time_utc = (rmc_fields[1]) ? atof(rmc_fields[1]) : 0;
    double rmc_date_utc = (rmc_fields[9]) ? atof(rmc_fields[9]) : 0;

    if (rmc_time_utc == 0 || rmc_date_utc == 0)
        return false;

    /* ---------- GGA 파싱 ---------- */
    char *gga_fields[15] = {0};
    idx = 0;

    token = strtok_r(gga, ",", &saveptr_gga);
    while (token != NULL && idx < 15) {
        gga_fields[idx++] = token;
        token = strtok_r(NULL, ",", &saveptr_gga);
    }

    int fix_quality = (gga_fields[6]) ? atoi(gga_fields[6]) : 0;
    int satellites  = (gga_fields[7]) ? atoi(gga_fields[7]) : 0;
    double hdop     = (gga_fields[8]) ? atof(gga_fields[8]) : 0;
    double altitude = (gga_fields[9]) ? atof(gga_fields[9]) : 0;

    double lat_raw  = (gga_fields[2]) ? atof(gga_fields[2]) : 0;
    char lat_ns     = (gga_fields[3]) ? gga_fields[3][0] : ' ';

    double lon_raw  = (gga_fields[4]) ? atof(gga_fields[4]) : 0;
    char lon_ew     = (gga_fields[5]) ? gga_fields[5][0] : ' ';

    /* ---------- Fix Type (GGA만 사용) ---------- */
    const char *fix_str = "No Fix";
    if (fix_quality == 1) fix_str = "GPS Fix";
    else if (fix_quality == 2) fix_str = "DGPS Fix";

    /* ---------- UTC → KST time 계산 ---------- */
    int t_int = (int)rmc_time_utc;
    int d_int = (int)rmc_date_utc;

    struct tm t = {0};
    t.tm_hour = t_int / 10000;
    t.tm_min  = (t_int / 100) % 100;
    t.tm_sec  = t_int % 100;
    t.tm_mday = d_int / 10000;
    t.tm_mon  = (d_int / 100 % 100) - 1;
    t.tm_year = (d_int % 100) + 100;

    time_t utc_timestamp = mktime(&t); 
    utc_timestamp += 9 * 3600;        // KST
    struct tm tm_kst;
    localtime_r(&utc_timestamp, &tm_kst);

    char kst_str[20];
    strftime(kst_str, sizeof(kst_str), "%Y-%m-%d %H:%M:%S", &tm_kst);

    /* ---------- 좌표 변환 ---------- */
    double lat = nmea_to_decimal(lat_raw);
    if (lat_ns == 'S') lat *= -1.0;

    double lon = nmea_to_decimal(lon_raw);
    if (lon_ew == 'W') lon *= -1.0;

    /* ---------- 출력 ---------- */
    ESP_LOGI(TAG_GPS, "-------------------------------");
    ESP_LOGI(TAG_GPS, "[GPS FIX]");
    ESP_LOGI(TAG_GPS, "Time (KST): %s", kst_str);
    ESP_LOGI(TAG_GPS, "Latitude  : %f° %c", lat, lat_ns);
    ESP_LOGI(TAG_GPS, "Longitude : %f° %c", lon, lon_ew);
    ESP_LOGI(TAG_GPS, "Altitude  : %.1f m", altitude);
    ESP_LOGI(TAG_GPS, "Satellites: %d", satellites);
    ESP_LOGI(TAG_GPS, "HDOP      : %.2f", hdop);
    ESP_LOGI(TAG_GPS, "Fix Type  : %s", fix_str);
    ESP_LOGI(TAG_GPS, "-------------------------------");

    /* ---------- BLE 업데이트 ---------- */
    char buf[64];
    snprintf(buf, sizeof(buf), "%.5f,%.5f (%s)", lat, lon, kst_str);

    ble_set_gps_value(buf);

    return true;
}

/* ============================================================
 * GPS Read Task
 * ============================================================ */
static void gps_read_task(void *arg)
{
    uint8_t *data = (uint8_t *) malloc(GPS_UART_BUF_SIZE);
    if (!data) {
        ESP_LOGE(TAG_GPS, "Failed to allocate UART buffer");
        vTaskDelete(NULL);
        return;
    }

    char line[256];
    int idx = 0;

    ESP_LOGI(TAG_GPS, "GPS Read Task started");

    while (1)
    {
        int len = uart_read_bytes(GPS_UART_NUM,
                                  data,
                                  GPS_UART_BUF_SIZE - 1,
                                  pdMS_TO_TICKS(100));

        if (len > 0)
        {
            for (int i = 0; i < len; i++) {
                char c = data[i];

                if (idx < sizeof(line) - 1)
                    line[idx++] = c;

                if (c == '\n') {
                    line[idx] = '\0';
                    if (line[0] == '$') {
                        parse_nmea_sentence(line);
                    }
                    idx = 0;
                }
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

/* ============================================================
 * TZ 설정
 * ============================================================ */
static void set_korea_timezone(void)
{
    setenv("TZ", "KST-9", 1);
    tzset();
}

/* ============================================================
 * Public: GPS Task Init
 * ============================================================ */
esp_err_t gps_task_init(void)
{
    set_korea_timezone();

    if (init_gps_uart() != ESP_OK)
        return ESP_FAIL;

    BaseType_t res = xTaskCreate(
        gps_read_task,
        "gps_read_task",
        GPS_TASK_STACK_SIZE,
        NULL,
        5,
        &xGpsTaskHandle
    );

    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}
