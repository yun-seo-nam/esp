#include "trigger.h"    // 1. 자신의 헤더 파일
#include "common.h"     // 2. 핀 번호, 로그 태그
#include "freertos/FreeRTOS.h" // 3. FreeRTOS 필수 헤더
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"    // 4. GPIO
#include "esp_timer.h"    // 5. 고해상도 타이머
#include "esp_log.h"      // 6. 로그

// --- 디바운싱(채터링) 방지 설정 ---
// (10,000us = 10ms)
#define DEBOUNCE_TIME_US (10 * 1000) 

// --- 모듈 내부에서만 사용할 변수 (static) ---

// ISR -> Task로 데이터를 전달하기 위한 큐
static QueueHandle_t event_queue = NULL;

// 디바운싱을 위한 '마지막 인터럽트 시간' 변수
static volatile int64_t last_trigger_interrupt_time_us = 0;

// 큐로 전달할 데이터 구조체
typedef enum {
    EVENT_TRIGGER, // 0
    EVENT_PPS      // 1
} event_type_t;

typedef struct {
    event_type_t type;
    int64_t time_us;
} time_event_t;


// --- 모듈 내부에서만 사용할 함수 (static) ---

/**
 * @brief GPIO 인터럽트 핸들러 (ISR) - Trigger 핀 (디바운싱 적용됨)
 */
static void IRAM_ATTR trigger_isr_handler(void* arg)
{
    int64_t now_us = esp_timer_get_time();

    if ((now_us - last_trigger_interrupt_time_us) < DEBOUNCE_TIME_US) {
        return; // 채터링 무시
    }
    last_trigger_interrupt_time_us = now_us;

    time_event_t event = {
        .type = EVENT_TRIGGER,
        .time_us = now_us
    };
    xQueueSendFromISR(event_queue, &event, NULL);
}

/**
 * @brief GPIO 인터럽트 핸들러 (ISR) - PPS 핀
 */
static void IRAM_ATTR pps_isr_handler(void* arg)
{
    int64_t now_us = esp_timer_get_time();
    time_event_t event = {
        .type = EVENT_PPS,
        .time_us = now_us
    };
    xQueueSendFromISR(event_queue, &event, NULL);
}

/**
 * @brief 메인 로직을 처리하는 별도의 Task
 */
static void timer_logic_task(void* arg)
{
    time_event_t received_event;
    int64_t last_trigger_time_us = 0; 
    int64_t last_pps_time_us = 0;     

    ESP_LOGI(TAG_TRIGGER, "타이머 로직 태스크 시작. 이벤트 대기 중...");

    while (1) {
        if (xQueueReceive(event_queue, &received_event, portMAX_DELAY)) {
            
            if (received_event.type == EVENT_TRIGGER) {
                last_trigger_time_us = received_event.time_us;
                ESP_LOGI(TAG_TRIGGER, "Trigger 감지됨: %lld us", last_trigger_time_us);
            
            } else if (received_event.type == EVENT_PPS) {
                int64_t current_pps_time_us = received_event.time_us;
                ESP_LOGI(TAG_TRIGGER, "PPS 감지됨: %lld us", current_pps_time_us);

                if (last_trigger_time_us > last_pps_time_us) {
                    int64_t offset_us = current_pps_time_us - last_trigger_time_us;
                    ESP_LOGW(TAG_TRIGGER, "===== OFFSET 계산됨: %lld us =====", offset_us);
                } else {
                    ESP_LOGI(TAG_TRIGGER, "이번 주기엔 새 트리거가 없어 계산을 건너뜁니다.");
                }
                last_pps_time_us = current_pps_time_us;
            }
        }
    }
}

// --- trigger.h 에 선언된 공개 함수 ---

/**
 * @brief Trigger/PPS 동기화 로직 초기화
 */
esp_err_t trigger_sync_init(void)
{
    ESP_LOGI(TAG_TRIGGER, "Trigger/PPS 동기화 모듈 초기화 시작...");
    
    // 1. [⭐️ 추가됨] GPIO 설정 (모듈이 스스로 하도록 변경)
    ESP_LOGI(TAG_APP, "Trigger/PPS 핀 초기화 중...");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE;      // 상승 에지
    io_conf.pin_bit_mask = (1ULL << PIN_TRIGGER_INPUT) | (1ULL << PIN_PPS_INPUT); 
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;   
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);

    // 2. 이벤트 큐(Queue) 생성
    event_queue = xQueueCreate(10, sizeof(time_event_t));
    if (event_queue == NULL) {
        ESP_LOGE(TAG_TRIGGER, "이벤트 큐 생성 실패!");
        return ESP_FAIL;
    }

    // 3. GPIO 인터럽트 서비스 설치
    esp_err_t isr_service_err = gpio_install_isr_service(0);
    if (isr_service_err != ESP_OK && isr_service_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_TRIGGER, "GPIO ISR 서비스 설치 실패: %s", esp_err_to_name(isr_service_err));
        return ESP_FAIL;
    }

    // 4. 각 핀에 ISR 핸들러 등록
    if (gpio_isr_handler_add(PIN_TRIGGER_INPUT, trigger_isr_handler, (void*) PIN_TRIGGER_INPUT) != ESP_OK) {
        ESP_LOGE(TAG_TRIGGER, "Trigger ISR 핸들러 등록 실패");
        return ESP_FAIL;
    }
    if (gpio_isr_handler_add(PIN_PPS_INPUT, pps_isr_handler, (void*) PIN_PPS_INPUT) != ESP_OK) {
        ESP_LOGE(TAG_TRIGGER, "PPS ISR 핸들러 등록 실패");
        return ESP_FAIL;
    }

    // 5. 계산 로직을 처리할 별도의 태스크 생성
    if (xTaskCreate(timer_logic_task, "timer_logic_task", 4096, NULL, 10, NULL) != pdPASS) {
        ESP_LOGE(TAG_TRIGGER, "timer_logic_task 태스크 생성 실패");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_TRIGGER, "Trigger/PPS 동기화 모듈 초기화 완료.");
    return ESP_OK;
}