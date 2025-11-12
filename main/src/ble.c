#include "ble.h"

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"

#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "esp_bt.h"   
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

esp_err_t esp_nimble_hci_and_controller_init(void);

static const char *TAG = "BLE_NIMBLE";

static int64_t  s_offset_us = 0;
static char     s_gps_str[64] = "Waiting for GPS...";
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_logs_val_handle = 0;

/* ===================================
 * UUID 정의
 * =================================== */
static const ble_uuid128_t UUID_SVC_TRIGGER =
    BLE_UUID128_INIT(0xFB,0x34,0x9B,0x5F,0x80,0x00,0x00,0x80,
                     0x00,0x10,0x00,0x00,0xAA,0x00,0x00,0x00);
static const ble_uuid128_t UUID_CHAR_OFFSET =
    BLE_UUID128_INIT(0xFB,0x34,0x9B,0x5F,0x80,0x00,0x00,0x80,
                     0x00,0x10,0x00,0x01,0xAA,0x00,0x00,0x00);
static const ble_uuid128_t UUID_CHAR_GPS =
    BLE_UUID128_INIT(0xFB,0x34,0x9B,0x5F,0x80,0x00,0x00,0x80,
                     0x00,0x10,0x00,0x02,0xAA,0x00,0x00,0x00);
static const ble_uuid128_t UUID_CHAR_LOGS =
    BLE_UUID128_INIT(0xFB,0x34,0x9B,0x5F,0x80,0x00,0x00,0x80,
                     0x00,0x10,0x00,0x03,0xAA,0x00,0x00,0x00);

/* ===================================
 * GATT 접근 콜백 함수
 * =================================== */
// offset 값 전송
 static int read_offset_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return os_mbuf_append(ctxt->om, &s_offset_us, sizeof(s_offset_us)) == 0
           ? 0 : BLE_ATT_ERR_UNLIKELY;
}

// gps 문자열 전송
static int read_gps_cb(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t len = strlen(s_gps_str);
    return os_mbuf_append(ctxt->om, s_gps_str, len) == 0
           ? 0 : BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC_TRIGGER.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid=&UUID_CHAR_OFFSET.u, .access_cb=read_offset_cb, .flags=BLE_GATT_CHR_F_READ },
            { .uuid=&UUID_CHAR_GPS.u,    .access_cb=read_gps_cb,    .flags=BLE_GATT_CHR_F_READ },
            { .uuid=&UUID_CHAR_LOGS.u,   .access_cb=NULL, .flags=BLE_GATT_CHR_F_NOTIFY,
              .val_handle=&s_logs_val_handle,
              .descriptors=(struct ble_gatt_dsc_def[]){
                  { .uuid=BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                    .att_flags=BLE_ATT_F_READ|BLE_ATT_F_WRITE,
                    .access_cb=NULL }, {0}
              }},
            {0}
        },
    },
    {0}
};

/* ===================================
 * GAP 이벤트
 * =================================== */
static int gap_event(struct ble_gap_event *ev, void *arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0) {
            s_conn_handle = ev->connect.conn_handle;
            ESP_LOGI(TAG, "Connected (handle=%u)", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "Connect failed; restart advertising");
            ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                              &(struct ble_gap_adv_params){
                                  .conn_mode=BLE_GAP_CONN_MODE_UND,
                                  .disc_mode=BLE_GAP_DISC_MODE_GEN},
                              gap_event, NULL);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=0x%02X", ev->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                          &(struct ble_gap_adv_params){
                              .conn_mode=BLE_GAP_CONN_MODE_UND,
                              .disc_mode=BLE_GAP_DISC_MODE_GEN},
                          gap_event, NULL);
        break;

    default:
        break;
    }
    return 0;
}

/* ===================================
 * 광고 시작
 * =================================== */
static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    struct ble_gap_adv_params adv = {0};

    const char *name = ble_svc_gap_device_name();
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv, gap_event, NULL);
    ESP_LOGI(TAG, "Advertising as '%s'", name);
}

/* ===================================
 * BLE 동기화 콜백
 * =================================== */
static void ble_on_sync(void)
{
    uint8_t addr_type;
    uint8_t addr_val[6] = {0};

    ble_hs_id_infer_auto(0, &addr_type);
    ble_hs_id_copy_addr(addr_type, addr_val, NULL);
    ESP_LOGI(TAG, "Addr: %02X:%02X:%02X:%02X:%02X:%02X",
             addr_val[5],addr_val[4],addr_val[3],addr_val[2],addr_val[1],addr_val[0]);

    ble_svc_gap_device_name_set("nimble");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    start_advertising();
}

/* ===================================
 * ble_hs_cfg 전역
 * =================================== */
struct ble_hs_cfg ble_hs_cfg = {
    .reset_cb = NULL,
    .sync_cb = ble_on_sync,
    .store_status_cb = ble_store_util_status_rr,
};

/* ===================================
 * 호스트 Task
 * =================================== */
void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "Host task start");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ===================================
 * Public API
 * =================================== */
esp_err_t ble_nimble_init(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 🔹 여기서 HCI + 컨트롤러 초기화  <-- 이 부분이 문제입니다.
    // ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init()); // <-- 이 줄을 주석 처리하거나 삭제하세요!

    // 🔹 NimBLE 호스트 초기화 (이 함수가 컨트롤러 초기화까지 모두 수행합니다)
    ESP_ERROR_CHECK(nimble_port_init());

    // 🔹 호스트 task 실행
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI("BLE", "NimBLE initialized successfully");
    return ESP_OK;
}

void ble_set_offset_value(int64_t v) { s_offset_us = v; }
void ble_set_gps_value(const char *gps)
{
    strncpy(s_gps_str, gps, sizeof(s_gps_str)-1);
    s_gps_str[sizeof(s_gps_str)-1] = '\0';
}
