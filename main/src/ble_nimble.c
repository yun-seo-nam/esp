#include "ble_nimble.h" // 1. 우리가 방금 만든 헤더

// 2. ⭐️ 필요한 모든 헤더 파일들 ⭐️
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "string.h"
#include "assert.h"         // 'assert()' 함수
// ⭐️ 'esp_nimble_hci.h' 삭제 -> 'esp_bt.h' 추가
#include "esp_bt.h"

// 3. ⭐️ 네가 원했던 '고정 핀 코드' ⭐️
#define STATIC_PASSKEY 910101

// 4. ⭐️ 모든 static 변수와 UUID 정의 ⭐️
static const char *TAG = "BLE_NIMBLE";

// "Trigger 정보 서비스"
static const ble_uuid128_t gatt_svc_trigger_uuid = 
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 
                     0x10, 0x00, 0x00, 0x01, 0xAA, 0x00, 0x00, 0x00);
// "Trigger Offset" 특성
static const ble_uuid128_t gatt_char_offset_uuid = 
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 
                     0x10, 0x00, 0x00, 0x02, 0xAA, 0x00, 0x00, 0x00);
// "GPS 정보" 특성
static const ble_uuid128_t gatt_char_gps_uuid = 
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 
                     0x10, 0x00, 0x00, 0x03, 0xAA, 0x00, 0x00, 0x00);
// "Logs" 특성
static const ble_uuid128_t gatt_char_logs_uuid = 
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 
                     0x10, 0x00, 0x00, 0x04, 0xAA, 0x00, 0x00, 0x00);

// 데이터 저장 변수
static int64_t g_current_offset_us = 0;
static char g_current_gps_str[64] = "Waiting for GPS...";

// 5. ⭐️ 모든 함수 선언 (내부용 static 함수들) ⭐️
static int gatt_svr_svc_access(uint16_t conn_handle, uint16_t attr_handle, 
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_start_advertising(void);
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_on_sync(void);
static void ble_on_reset(int reason);
static void ble_host_task(void *param);


// 6. ⭐️ GATT 콜백 함수 (데이터 읽기/쓰기) ⭐️
static int gatt_svr_svc_access(uint16_t conn_handle, uint16_t attr_handle, 
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc = 0;
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &gatt_char_offset_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ESP_LOGI(TAG, "Offset 값 읽기 요청: %lld", g_current_offset_us);
            rc = os_mbuf_append(ctxt->om, &g_current_offset_us, sizeof(g_current_offset_us));
            return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    
    if (ble_uuid_cmp(uuid, &gatt_char_gps_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ESP_LOGI(TAG, "GPS 값 읽기 요청: %s", g_current_gps_str);
            rc = os_mbuf_append(ctxt->om, g_current_gps_str, strlen(g_current_gps_str));
            return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    
    if (ble_uuid_cmp(uuid, &gatt_char_logs_uuid.u) == 0) {
        return 0; // Notify 전용
    }

    ESP_LOGW(TAG, "알 수 없는 UUID 접근!");
    return BLE_ATT_ERR_UNLIKELY;
}

// 7. ⭐️ GATT '메뉴판' 정의 (고정 핀 코드용) ⭐️
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svc_trigger_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &gatt_char_offset_uuid.u, .access_cb = gatt_svr_svc_access, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC, },
            { .uuid = &gatt_char_gps_uuid.u,    .access_cb = gatt_svr_svc_access, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC, },
            { .uuid = &gatt_char_logs_uuid.u,   .access_cb = gatt_svr_svc_access, .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC, },
            { 0, }, // 마지막
        },
    },
    { 0, }, // 마지막
};

// 8. ⭐️ GAP 광고 시작 함수 ⭐️
static void ble_start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen((char *)fields.name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "광고 필드 설정 실패! rc=%d", rc); return; }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 6400; // 4초
    adv_params.itvl_max = 6400; // 4초

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) { ESP_LOGE(TAG, "광고 시작 실패! rc=%d", rc); return; }

    ESP_LOGI(TAG, "BLE 광고 시작 (이름: %s)", (char*)fields.name);
}

// 9. ⭐️ GAP 이벤트 핸들러 (네 예제 코드의 '고정 핀' 로직 적용) ⭐️
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE 연결됨 (status=%d)", event->connect.status);
        if (event->connect.status != 0) {
            ble_start_advertising(); // 연결 실패, 다시 광고
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE 연결 끊김 (reason=%d)", event->disconnect.reason);
        ble_start_advertising(); // 연결 끊김, 다시 광고
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "연결 암호화 완료!");
        } else {
            ESP_LOGE(TAG, "연결 암호화 실패, status: %d", event->enc_change.status);
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            if (rc == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc != 0) {
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        ble_store_util_delete_peer(&desc.peer_id_addr);
        ESP_LOGI(TAG, "중복 페어링: 기존 본딩 삭제. 다시 시도.");
        return BLE_GAP_REPEAT_PAIRING_RETRY; // 페어링 재시도

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY ACTION 이벤트 (action=%d)", event->passkey.params.action);
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            struct ble_sm_io pkey = {0};
            pkey.action = event->passkey.params.action;
            pkey.passkey = STATIC_PASSKEY; // ⭐️ 네가 원한 910101
            
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0) {
                ESP_LOGE(TAG, "고정 핀 코드 주입 실패, rc: %d", rc);
            } else {
                ESP_LOGI(TAG, "고정 핀 코드 [ %ld ] 주입 완료", (long)pkey.passkey);
            }
        } else {
             ESP_LOGW(TAG, "지원하지 않는 Passkey action: %d", event->passkey.params.action);
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe 이벤트; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
        break;

    default:
        break;
    }
    return 0;
}

// 10. ⭐️ NimBLE 스택 콜백 함수들 ⭐️
static void ble_on_sync(void)
{
    int rc;
    rc = ble_hs_util_ensure_addr(0); // MAC 주소 설정
    assert(rc == 0);
    rc = ble_svc_gap_device_name_set("WiEW_nam"); // 이름 설정
    assert(rc == 0);
    ble_start_advertising(); // 광고 시작
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE 스택 리셋! reason: %d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task 시작");
    nimble_port_run(); // 이 함수는 종료되지 않음
    nimble_port_freertos_deinit();
}

// 11. ⭐️ 외부(main.c)에서 호출할 함수들 ⭐️
void ble_gatt_set_offset_value(int64_t offset_us)
{
    g_current_offset_us = offset_us;
}

void ble_gatt_set_gps_value(const char* gps_str)
{
    strncpy(g_current_gps_str, gps_str, sizeof(g_current_gps_str) - 1);
    g_current_gps_str[sizeof(g_current_gps_str) - 1] = '\0';
}

// 12. ⭐️ BLE 스택 초기화 (네가 쓰지 말라고 한 함수 뺐어!) ⭐️
esp_err_t ble_nimble_stack_init(void)
{
    esp_err_t ret;
    int rc; 

    // NVS 초기화
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ⭐️ --- 'esp_nimble_hci...' 함수 대신 수동으로 초기화 ---
    // (esp_bt.h 헤더 파일이 필요해)
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "BT 컨트롤러 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "BT 컨트롤러 활성화 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BT 컨트롤러 수동 초기화 완료.");
    // ⭐️ --- 여기까지 ---

    nimble_port_init(); // NimBLE 호스트 스택 초기화

    // 콜백 등록
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // GATT 서비스 등록
    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);
    
    // ⭐️ 보안 설정 ('고정 핀'을 사용하도록 수정) ⭐️
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY; // "내가 핀 코드를 낼게"
    ble_hs_cfg.sm_sc = 1; // Secure Connections
    ble_hs_cfg.sm_mitm = 1; // MITM 방어 활성화
    ble_hs_cfg.sm_bonding = 1; 

    // NimBLE 태스크 시작
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "NimBLE 스택 초기화 완료. (고정 핀 코드 모드, 수동 컨트롤러)");
    return ESP_OK;
}