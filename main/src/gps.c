#include "gps.h"        
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

// ⭐️ FIX 1: 'timegm'의 extern 선언을 삭제합니다.
// extern time_t timegm(struct tm *tm); 

static bool print_gps_info(char *gga, char *rmc, char *gsa);
static void set_korea_timezone(void); 

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
static char g_gsa_line[128] = {0};

static bool g_has_gga = false;
static bool g_has_rmc = false;
static bool g_has_gsa = false;

/* ============================================================
 * Private 함수
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

static void parse_nmea_sentence(const char *line)
{
    ESP_LOGD(TAG_GPS, "Raw: %s", line);

    bool check_for_batch_complete = false;

    if (strstr(line, "$GNGGA") || strstr(line, "$GPGGA")) {
        strncpy(g_gga_line, line, sizeof(g_gga_line) - 1);
        g_gga_line[sizeof(g_gga_line) - 1] = '\0'; 
        g_has_gga = true; 
        check_for_batch_complete = true; 
    }

    else if (strstr(line, "$GNRMC") || strstr(line, "$GPRMC")) {
        strncpy(g_rmc_line, line, sizeof(g_rmc_line) - 1);
        g_rmc_line[sizeof(g_rmc_line) - 1] = '\0';
        g_has_rmc = true; 
        check_for_batch_complete = true;
    }

    else if (strstr(line, "$GNGSA") || strstr(line, "$GPGSA")) {
        strncpy(g_gsa_line, line, sizeof(g_gsa_line) - 1);
        g_gsa_line[sizeof(g_gsa_line) - 1] = '\0';
        g_has_gsa = true; 
        check_for_batch_complete = true;
    }

    if (check_for_batch_complete) 
    {
        if (g_has_gga && g_has_rmc && g_has_gsa)
        {
            print_gps_info(g_gga_line, g_rmc_line, g_gsa_line);
            
            g_has_gga = false;
            g_has_rmc = false;
            g_has_gsa = false;
            
            g_gga_line[0] = '\0';
            g_rmc_line[0] = '\0';
            g_gsa_line[0] = '\0';
        }
    }
}

static double nmea_to_decimal(double nmea_val)
{
    int degrees = (int)(nmea_val / 100);
    double minutes = nmea_val - (degrees * 100);
    double decimal = degrees + (minutes / 60.0);
    return decimal;
}

static bool print_gps_info(char *gga, char *rmc, char *gsa)
{
    char *token;
    char *rmc_fields[14] = {0}; 
    int i = 0;

    token = strtok(rmc, ","); 
    while(token != NULL && i < 14) {
        rmc_fields[i++] = token;
        token = strtok(NULL, ",");
    }

    char rmc_status = 'V';
    if (rmc_fields[2] && rmc_fields[2][0]) {
        rmc_status = rmc_fields[2][0];
    }
    
    if (rmc_status == 'V') {
        ESP_LOGI(TAG_GPS, "-------------------------------");
        ESP_LOGI(TAG_GPS, "[NO FIX] Waiting for satellites...");
        ESP_LOGI(TAG_GPS, "-------------------------------");
        return false; 
    }

    double rmc_time_utc = (rmc_fields[1]) ? atof(rmc_fields[1]) : 0;
    double rmc_date_utc = (rmc_fields[9]) ? atof(rmc_fields[9]) : 0;

    if (rmc_time_utc == 0 || rmc_date_utc == 0) {
        return false; 
    }

    char *gga_fields[15] = {0};
    i = 0;
    token = strtok(gga, ","); 
    while(token != NULL && i < 15) {
        gga_fields[i++] = token;
        token = strtok(NULL, ",");
    }
    
    int fix_quality   = (gga_fields[6]) ? atoi(gga_fields[6]) : 0;
    int satellites    = (gga_fields[7]) ? atoi(gga_fields[7]) : 0;
    double hdop       = (gga_fields[8]) ? atof(gga_fields[8]) : 0;
    double altitude   = (gga_fields[9]) ? atof(gga_fields[9]) : 0;
    double lat_nmea = (gga_fields[2]) ? atof(gga_fields[2]) : 0;
    char lat_ns     = (gga_fields[3] && gga_fields[3][0]) ? gga_fields[3][0] : ' ';
    double lon_nmea = (gga_fields[4]) ? atof(gga_fields[4]) : 0;
    char lon_ew     = (gga_fields[5] && gga_fields[5][0]) ? gga_fields[5][0] : ' ';

    char *gsa_fields[20] = {0}; 
    i = 0;
    token = strtok(gsa, ","); 
    while(token != NULL && i < 20) {
        gsa_fields[i++] = token;
        token = strtok(NULL, ",");
    }

    int fix_type = (gsa_fields[2]) ? atoi(gsa_fields[2]) : 1;

    // --- 1. KST 시간/날짜 변환 ---
    struct tm t = {0};
    int time_int = (int)rmc_time_utc;
    int date_int = (int)rmc_date_utc;
    
    t.tm_hour = (time_int / 10000);
    t.tm_min  = (time_int / 100) % 100;
    t.tm_sec  = time_int % 100;
    
    t.tm_mday = (date_int / 10000);
    t.tm_mon  = ((date_int / 100) % 100) - 1; 
    t.tm_year = (date_int % 100) + 100;     
    
    // ⭐️ FIX 2: mktime()을 사용하고 KST 오프셋(9시간 = 32400초)을 수동으로 더함
    time_t utc_time = mktime(&t) + (9 * 3600); 
    
    struct tm tm_kst;
    localtime_r(&utc_time, &tm_kst); 
    
    char kst_time_str[20];
    strftime(kst_time_str, sizeof(kst_time_str), "%Y-%m-%d %H:%M:%S", &tm_kst);
    
    // --- 2. 좌표 변환 ---
    double latitude_decimal = nmea_to_decimal(lat_nmea);
    double longitude_decimal = nmea_to_decimal(lon_nmea);
    if (lat_ns == 'S') latitude_decimal *= -1.0;
    if (lon_ew == 'W') longitude_decimal *= -1.0;

    // --- 3. Fix Type 문자열 변환 ---
    const char *fix_str = "Unknown";
    if (fix_type == 1) fix_str = "No Fix";
    else if (fix_type == 2) fix_str = "2D Fix";
    else if (fix_type == 3) fix_str = "3D Fix";
    
    // --- 4. 최종 출력 ---
    ESP_LOGI(TAG_GPS, "-------------------------------");
    ESP_LOGI(TAG_GPS, "[GPS FIX]");
    ESP_LOGI(TAG_GPS, "Time (KST): %s", kst_time_str);
    ESP_LOGI(TAG_GPS, "Latitude  : %f° %c", latitude_decimal, lat_ns);
    ESP_LOGI(TAG_GPS, "Longitude : %f° %c", longitude_decimal, lon_ew);
    ESP_LOGI(TAG_GPS, "Altitude  : %.1f m", altitude);
    ESP_LOGI(TAG_GPS, "Satellites: %d", satellites);
    ESP_LOGI(TAG_GPS, "HDOP      : %.2f", hdop);
    ESP_LOGI(TAG_GPS, "Fix Type  : %s (GGA_Fix:%d, GSA_Fix:%d)", fix_str, fix_quality, fix_type);
    ESP_LOGI(TAG_GPS, "-------------------------------");

    return true; 
}


static void gps_read_task(void *arg)
{
    uint8_t *data = (uint8_t *) malloc(GPS_UART_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG_GPS, "Failed to allocate memory for UART buffer. Task aborting.");
        vTaskDelete(NULL);
        return;
    }

    char line_buffer[256];
    int line_idx = 0;

    ESP_LOGI(TAG_GPS, "GPS Read Task started. Buffering NMEA lines...");

    while (1) 
    {
        int len = uart_read_bytes(GPS_UART_NUM, 
                                  data, 
                                  GPS_UART_BUF_SIZE - 1, 
                                  pdMS_TO_TICKS(100));

        if (len < 0) {
            ESP_LOGE(TAG_GPS, "UART read error!");
        } 
        else if (len > 0) 
        {
            for (int i = 0; i < len; i++)
            {
                char c = data[i]; 
                if (line_idx < (sizeof(line_buffer) - 1)) {
                    line_buffer[line_idx++] = c;
                } else {
                    ESP_LOGE(TAG_GPS, "Line buffer overflow! Clearing.");
                    line_idx = 0; 
                }

                if (c == '\n')
                {
                    line_buffer[line_idx] = '\0'; 
                    if (line_buffer[0] == '$') {
                        parse_nmea_sentence(line_buffer);
                    }
                    line_idx = 0; 
                }
            } 
        } 
    }
    
    free(data);
    vTaskDelete(NULL);
}

static void set_korea_timezone(void)
{
    setenv("TZ", "KST-9", 1);
    tzset();
    ESP_LOGI(TAG_APP, "Timezone set to KST (UTC+9)");
}

/* ============================================================
 * Public 함수 (gps.h 에 선언됨)
 * ============================================================ */

esp_err_t gps_task_init(void)
{
    set_korea_timezone();

    esp_err_t uart_init_err = init_gps_uart();
    if (uart_init_err != ESP_OK) {
        ESP_LOGE(TAG_GPS, "Failed to initialize GPS UART.");
        return ESP_FAIL;
    }

    BaseType_t res = xTaskCreate(
        gps_read_task,
        "gps_read_task",
        GPS_TASK_STACK_SIZE, 
        NULL,
        5, 
        &xGpsTaskHandle 
    );

    if (res == pdPASS) {
        ESP_LOGI(TAG_APP, "GPS Read Task created successfully.");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_APP, "Failed to create GPS Read Task.");
        return ESP_FAIL;
    }
}