/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "ble_manager.h"
#include "common.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

extern "C"
{
#include "gap.h"
#include "gatt_svc.h"
    void ble_store_config_init(void);
    void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
    void gatt_svr_subscribe_cb(struct ble_gap_event *event);
}

// Tag for logging

// URI for advertising
static uint8_t esp_uri[] = {0x17, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};

// Singleton instance
static BleManager *s_ble_manager_instance = nullptr;

BleManager &BleManager::getInstance()
{
    if (s_ble_manager_instance == nullptr)
    {
        s_ble_manager_instance = new BleManager();
    }
    return *s_ble_manager_instance;
}

BleManager::BleManager()
    : m_ownAddrType(0), m_connected(false), m_conn_handle(BLE_HS_CONN_HANDLE_NONE)
{
    memset(m_addrVal, 0, sizeof(m_addrVal));
}

BleManager::~BleManager()
{
    s_ble_manager_instance = nullptr;
}

esp_err_t BleManager::init()
{
    esp_err_t ret;

    /* Initialize NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize NimBLE stack */
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d", ret);
        return ret;
    }

    /* Initialize GAP service */
    ret = gap_init();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", ret);
        return ret;
    }

    /* Initialize GATT server */
    ret = gatt_svc_init();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", ret);
        return ret;
    }

    /* Configure host */
    configureHost();

    /* Start NimBLE host task */
    xTaskCreate([](void *param)
                {
        ESP_LOGI(TAG, "nimble host task has been started!");
        nimble_port_run();
        vTaskDelete(NULL); }, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

    return ESP_OK;
}

void BleManager::configureHost()
{
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = [](int reason)
    {
        ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
    };
    ble_hs_cfg.sync_cb = []()
    {
        BleManager::getInstance().startAdvertising();
    };
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize BLE store */
    ble_store_config_init();
}

void BleManager::startAdvertising()
{
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* Ensure we have a proper BT identity address */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    /* Figure out BT address type */
    rc = ble_hs_id_infer_auto(0, &m_ownAddrType);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    /* Copy device address */
    rc = ble_hs_id_copy_addr(m_ownAddrType, m_addrVal, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }

    /* Print device address */
    char addr_str[18] = {0};
    formatAddr(addr_str, m_addrVal);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    /* Set advertising flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* Set device tx power */
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    /* Set device appearance */
    adv_fields.appearance = 0x0200; // BLE_GAP_APPEARANCE_GENERIC_TAG
    adv_fields.appearance_is_present = 1;

    /* Set device LE role */
    adv_fields.le_role = 0x00; // BLE_GAP_LE_ROLE_PERIPHERAL
    adv_fields.le_role_is_present = 1;

    /* Set service UUIDs */
    static const ble_uuid16_t adv_uuid_list[] = {BLE_UUID16_INIT(0x1815)};
    adv_fields.uuids16 = adv_uuid_list;
    adv_fields.num_uuids16 = 1;
    adv_fields.uuids16_is_complete = 1;

    /* Set advertising fields */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    /* Set device address in response */
    rsp_fields.device_addr = m_addrVal;
    rsp_fields.device_addr_type = m_ownAddrType;
    rsp_fields.device_addr_is_present = 1;

    /* Set URI */
    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);

    /* Set advertising interval */
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    /* Set scan response fields */
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }

    /* Set non-connectable and general discoverable mode */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Set advertising interval */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    /* Start advertising */
    rc = ble_gap_adv_start(m_ownAddrType, NULL, BLE_HS_FOREVER, &adv_params,
                           gapEventHandler, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}

int BleManager::gapEventHandler(struct ble_gap_event *event, void *arg)
{
    return BleManager::getInstance().handleGapEvent(event);
}

int BleManager::handleGapEvent(struct ble_gap_event *event)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        return handleConnect(event);

    case BLE_GAP_EVENT_DISCONNECT:
        return handleDisconnect(event);

    case BLE_GAP_EVENT_CONN_UPDATE:
        return handleConnUpdate(event);

    case BLE_GAP_EVENT_ADV_COMPLETE:
        return handleAdvComplete(event);

    case BLE_GAP_EVENT_NOTIFY_TX:
        return handleNotifyTx(event);

    case BLE_GAP_EVENT_SUBSCRIBE:
        return handleSubscribe(event);

    case BLE_GAP_EVENT_MTU:
        return handleMtu(event);

    default:
        break;
    }
    return 0;
}

int BleManager::handleConnect(struct ble_gap_event *event)
{
    int rc = 0;
    struct ble_gap_conn_desc desc;

    ESP_LOGI(TAG, "connection %s; status=%d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);

    if (event->connect.status == 0)
    {
        /* Find connection */
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "failed to find connection by handle, error code: %d", rc);
            return rc;
        }

        /* Print connection descriptor */
        printConnDesc(&desc);

        /* Update connection parameters */
        struct ble_gap_upd_params params = {
            .itvl_min = desc.conn_itvl,
            .itvl_max = desc.conn_itvl,
            .latency = 3,
            .supervision_timeout = desc.supervision_timeout,
            .min_ce_len = 0,
            .max_ce_len = 0};
        rc = ble_gap_update_params(event->connect.conn_handle, &params);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "failed to update connection parameters, error code: %d", rc);
            return rc;
        }

        /* Update connection status */
        m_connected = true;
        m_conn_handle = event->connect.conn_handle;
    }
    else
    {
        /* Connection failed, restart advertising */
        startAdvertising();
    }
    return rc;
}

int BleManager::handleDisconnect(struct ble_gap_event *event)
{
    ESP_LOGI(TAG, "disconnected from peer; reason=%d", event->disconnect.reason);

    /* Update connection status */
    m_connected = false;
    m_conn_handle = BLE_HS_CONN_HANDLE_NONE;

    /* Restart advertising */
    startAdvertising();
    return 0;
}

int BleManager::handleConnUpdate(struct ble_gap_event *event)
{
    int rc = 0;
    struct ble_gap_conn_desc desc;

    ESP_LOGI(TAG, "connection updated; status=%d", event->conn_update.status);

    rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to find connection by handle, error code: %d", rc);
        return rc;
    }
    printConnDesc(&desc);
    return rc;
}

int BleManager::handleAdvComplete(struct ble_gap_event *event)
{
    ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
    startAdvertising();
    return 0;
}

int BleManager::handleNotifyTx(struct ble_gap_event *event)
{
    if ((event->notify_tx.status != 0) && (event->notify_tx.status != BLE_HS_EDONE))
    {
        ESP_LOGI(TAG,
                 "notify event; conn_handle=%d attr_handle=%d "
                 "status=%d is_indication=%d",
                 event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                 event->notify_tx.status, event->notify_tx.indication);
    }
    return 0;
}

int BleManager::handleSubscribe(struct ble_gap_event *event)
{
    ESP_LOGI(TAG,
             "subscribe event; conn_handle=%d attr_handle=%d "
             "reason=%d prevn=%d curn=%d previ=%d curi=%d",
             event->subscribe.conn_handle, event->subscribe.attr_handle,
             event->subscribe.reason, event->subscribe.prev_notify,
             event->subscribe.cur_notify, event->subscribe.prev_indicate,
             event->subscribe.cur_indicate);

    /* Call GATT subscribe callback */
    gatt_svr_subscribe_cb(event);
    return 0;
}

int BleManager::handleMtu(struct ble_gap_event *event)
{
    ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
             event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
    return 0;
}

void BleManager::printConnDesc(struct ble_gap_conn_desc *desc)
{
    char addr_str[18] = {0};

    /* Connection handle */
    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

    /* Local ID address */
    formatAddr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s",
             desc->our_id_addr.type, addr_str);

    /* Peer ID address */
    formatAddr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type,
             addr_str);

    /* Connection info */
    ESP_LOGI(TAG,
             "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
             "encrypted=%d, authenticated=%d, bonded=%d\n",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

void BleManager::formatAddr(char *addr_str, uint8_t addr[])
{
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

int BleManager::setDeviceName(const char *name)
{
    int rc = ble_svc_gap_device_name_set(name);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d", name, rc);
    }
    return rc;
}

void BleManager::sendHeartRateIndication(void)
{
    // TODO: Implement heart rate indication
    ESP_LOGD(TAG, "sendHeartRateIndication called");
}