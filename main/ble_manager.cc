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
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "application.h"
#include "NVSManager.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#define TAG "BleManager"
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

// /* Automation IO service */
// static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
// static uint16_t ssid_chr_val_handle;
// static const ble_uuid16_t ssid_chr_uuid = BLE_UUID16_INIT(0x2A31);
// static uint16_t password_chr_val_handle;
// static const ble_uuid16_t password_chr_uuid = BLE_UUID16_INIT(0x2A32);
// static uint16_t connect_chr_val_handle;
// static const ble_uuid16_t connect_chr_uuid = BLE_UUID16_INIT(0x2A33);

// static uint16_t connect_status_chr_val_handle;
// static const ble_uuid16_t connect_status_chr_uuid = BLE_UUID16_INIT(0x2A21);
// static uint16_t mqtt_status_chr_val_handle;
// static const ble_uuid16_t mqtt_status_chr_uuid = BLE_UUID16_INIT(0x2A22);

// /* MQTT Configuration service */
// static const ble_uuid16_t mqtt_config_svc_uuid = BLE_UUID16_INIT(0x1889); // Custom UUID for MQTT Configuration Service
// static uint16_t mqtt_addr_chr_val_handle;
// static const ble_uuid16_t mqtt_addr_chr_uuid = BLE_UUID16_INIT(0x2B01); // Custom UUID for MQTT Server Address
// static uint16_t mqtt_username_chr_val_handle;
// static const ble_uuid16_t mqtt_username_chr_uuid = BLE_UUID16_INIT(0x2B02); // Custom UUID for MQTT Username
// static uint16_t mqtt_password_chr_val_handle;
// static const ble_uuid16_t mqtt_password_chr_uuid = BLE_UUID16_INIT(0x2B03); // Custom UUID for MQTT Password

// /* Magic Learning service */
// static const ble_uuid16_t magic_learning_svc_uuid = BLE_UUID16_INIT(0x1890); // Custom UUID for Magic Learning Service
// static uint16_t magic_enable_chr_val_handle;
// static const ble_uuid16_t magic_enable_chr_uuid = BLE_UUID16_INIT(0x2C01); // Custom UUID for Magic Enable characteristic
// /* GATT services table */

const ble_uuid16_t BleManager::magic_learning_svc_uuid = BLE_UUID16_INIT(0x1890); // Custom UUID for Magic Learning Service
const ble_uuid16_t BleManager::magic_enable_chr_uuid = BLE_UUID16_INIT(0x2C01);   // Custom UUID for Magic Enable characteristic
uint16_t BleManager::magic_enable_chr_val_handle = 0;                             // 初始化为0

const ble_uuid16_t BleManager::auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
uint16_t BleManager::ssid_chr_val_handle = 0;
const ble_uuid16_t BleManager::ssid_chr_uuid = BLE_UUID16_INIT(0x2A31);
uint16_t BleManager::password_chr_val_handle = 0;
const ble_uuid16_t BleManager::password_chr_uuid = BLE_UUID16_INIT(0x2A32);
uint16_t BleManager::connect_chr_val_handle = 0;
const ble_uuid16_t BleManager::connect_chr_uuid = BLE_UUID16_INIT(0x2A33);
uint16_t BleManager::connect_status_chr_val_handle = 0;
const ble_uuid16_t BleManager::connect_status_chr_uuid = BLE_UUID16_INIT(0x2A21);
uint16_t BleManager::mqtt_status_chr_val_handle = 0;
const ble_uuid16_t BleManager::mqtt_status_chr_uuid = BLE_UUID16_INIT(0x2A22);

const ble_uuid16_t BleManager::mqtt_config_svc_uuid = BLE_UUID16_INIT(0x1889); // Custom UUID for MQTT Configuration Service
uint16_t BleManager::mqtt_addr_chr_val_handle = 0;
const ble_uuid16_t BleManager::mqtt_addr_chr_uuid = BLE_UUID16_INIT(0x2B01); // Custom UUID for MQTT Server Address
uint16_t BleManager::mqtt_username_chr_val_handle = 0;
const ble_uuid16_t BleManager::mqtt_username_chr_uuid = BLE_UUID16_INIT(0x2B02); // Custom UUID for MQTT Username
uint16_t BleManager::mqtt_password_chr_val_handle = 0;
const ble_uuid16_t BleManager::mqtt_password_chr_uuid = BLE_UUID16_INIT(0x2B03); // Custom UUID for MQTT Password

const struct ble_gatt_svc_def BleManager::gatt_svr_svcs[] = {

    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &auto_io_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){/* WiFi SSID characteristic */
                                        {.uuid = &ssid_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                                         .val_handle = &ssid_chr_val_handle},
                                        {.uuid = &password_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                                         .val_handle = &password_chr_val_handle},
                                        {.uuid = &connect_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                                         .val_handle = &connect_chr_val_handle},
                                        {.uuid = &connect_status_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                                         .val_handle = &connect_status_chr_val_handle},
                                        {.uuid = &mqtt_status_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_READ,
                                         .val_handle = &mqtt_status_chr_val_handle},
                                        {0}},
    },

    /* MQTT Configuration service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &mqtt_config_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                /* MQTT Server Address characteristic */
                {
                    .uuid = &mqtt_addr_chr_uuid.u,
                    .access_cb = mqtt_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                    .val_handle = &mqtt_addr_chr_val_handle},
                /* MQTT Username characteristic */
                {
                    .uuid = &mqtt_username_chr_uuid.u,
                    .access_cb = mqtt_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                    .val_handle = &mqtt_username_chr_val_handle},
                /* MQTT Password characteristic */
                {
                    .uuid = &mqtt_password_chr_uuid.u,
                    .access_cb = mqtt_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                    .val_handle = &mqtt_password_chr_val_handle},
                {0} // End of characteristics array
            },
    },
    /* Magic Learning service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &magic_learning_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                /* Magic Enable characteristic */
                {
                    .uuid = &magic_enable_chr_uuid.u,
                    .access_cb = magic_learning_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                    .val_handle = &magic_enable_chr_val_handle},
                {0} // End of characteristics array
            },
    },

    {
        0, /* No more services. */
    },
};
esp_err_t BleManager::init()
{
    NVSManager nvsManager("storage");
    nvsManager.init();
    esp_err_t ret;

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

    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    ret = ble_gatts_count_cfg(gatt_svr_svcs);
    if (ret != 0)
    {
        return ret;
    }

    /* 3. Add GATT services */
    ret = ble_gatts_add_svcs(gatt_svr_svcs);
    if (ret != 0)
    {
        return ret;
    }

    /* Initialize GATT server */
    // ret = gatt_svc_init();
    // if (ret != 0)
    // {
    //     ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", ret);
    //     return ret;
    // }

    /* Configure host */
    configureHost();

    /* Start NimBLE host task */
    TaskHandle_t nimble_task_handle = NULL;
    BaseType_t task_created = xTaskCreate([](void *param)
                                          {
        ESP_LOGI(TAG, "nimble host task has been started!");
        nimble_port_run();
        vTaskDelete(NULL); }, "NimBLE Host", 4 * 1024, NULL, 5, &nimble_task_handle);

    if (task_created == pdPASS && nimble_task_handle != NULL)
    {
        if (esp_psram_get_size() > 0)
        {
            ESP_LOGI(TAG, "NimBLE task created, PSRAM is available for other allocations");
        }
        else
        {
            ESP_LOGW(TAG, "NimBLE task created, no PSRAM available");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create NimBLE host task");
        return ESP_ERR_NO_MEM;
    }

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

int BleManager::magic_learning_chr_access(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "magic_learning_chr_access called with conn_handle=%d, attr_handle=%d, op=%d",
             conn_handle, attr_handle, ctxt->op);

    /* Local variables */
    int rc;
    static bool magic_enabled = false; // 存储魔法启用状态

    /* Handle access events */
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Handle Magic Enable characteristic */
        goto error;

    /* Write characteristic event */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGI(TAG, "Magic learning characteristic write; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        }
        else
        {
            ESP_LOGI(TAG,
                     "Magic learning characteristic write by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Handle Magic Enable characteristic */
        if (attr_handle == magic_enable_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len != sizeof(bool))
            { // 布尔值应该正好是一个字节
                ESP_LOGE(TAG, "Invalid length for boolean value: %d", len);
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            // 从请求中读取布尔值
            bool new_magic_state;
            os_mbuf_copydata(ctxt->om, 0, len, &new_magic_state);

            // 更新魔法启用状态
            magic_enabled = new_magic_state;

            ESP_LOGI(TAG, "Magic learning state updated via BLE: %s",
                     magic_enabled ? "ENABLED" : "DISABLED");

            // 可以在这里添加实际的魔法学习启动/停止逻辑
            if (magic_enabled)
            {
                ESP_LOGI(TAG, "Starting magic learning process...");
                // 在这里可以添加启动魔法学习的代码
            }
            else
            {
                ESP_LOGI(TAG, "Stopping magic learning process...");
                // 在这里可以添加停止魔法学习的代码
            }

            return 0;
        }

        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(TAG,
             "unexpected access operation to Magic Learning characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
    return 0;
}

int BleManager::mqtt_chr_access(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg)
{
    NVSManager nvsManager("storage");
    nvsManager.init();

    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Handle MQTT Server Address characteristic */
        if (attr_handle == mqtt_addr_chr_val_handle)
        {
            std::string server_addr = nvsManager.readString("mqtt_server_addr");

            // 如果服务器地址为空，不发送任何内容
            if (server_addr.empty())
            {
                ESP_LOGW(TAG, "MQTT server address is empty, not sending to BLE client");
                goto error;
            }

            rc = os_mbuf_append(ctxt->om, server_addr.c_str(), server_addr.length());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        /* Handle MQTT Username characteristic */
        if (attr_handle == mqtt_username_chr_val_handle)
        {
            std::string username = nvsManager.readString("mqtt_username");

            // 如果用户名为空，不发送任何内容
            if (username.empty())
            {
                ESP_LOGW(TAG, "MQTT username is empty, not sending to BLE client");
                goto error;
            }

            rc = os_mbuf_append(ctxt->om, username.c_str(), username.length());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        /* Handle MQTT Password characteristic */
        if (attr_handle == mqtt_password_chr_val_handle)
        {
            std::string password = nvsManager.readString("mqtt_password");

            // 如果密码为空，不发送任何内容
            if (password.empty())
            {
                ESP_LOGW(TAG, "MQTT password is empty, not sending to BLE client");
                goto error;
            }

            rc = os_mbuf_append(ctxt->om, password.c_str(), password.length());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        goto error;

    /* Write characteristic event */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGI(TAG, "MQTT characteristic write; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        }
        else
        {
            ESP_LOGI(TAG,
                     "MQTT characteristic write by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Handle MQTT Server Address characteristic */
        if (attr_handle == mqtt_addr_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 255) // Max length for MQTT server address
            {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char server_addr[256] = {0};
            os_mbuf_copydata(ctxt->om, 0, len, server_addr);

            // 检查MQTT服务器地址是否为空或只包含空白字符
            bool isEmpty = true;
            for (int i = 0; i < len; i++)
            {
                if (server_addr[i] != ' ' && server_addr[i] != '\t' && server_addr[i] != '\n' && server_addr[i] != '\r')
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                ESP_LOGW(TAG, "Received empty MQTT server address via BLE, not storing");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            nvsManager.writeString("mqtt_server_addr", server_addr);

            return 0;
        }

        /* Handle MQTT Username characteristic */
        if (attr_handle == mqtt_username_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 64) // Max length for username
            {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char username[65] = {0};
            os_mbuf_copydata(ctxt->om, 0, len, username);

            // 检查MQTT用户名是否为空或只包含空白字符
            bool isEmpty = true;
            for (int i = 0; i < len; i++)
            {
                if (username[i] != ' ' && username[i] != '\t' && username[i] != '\n' && username[i] != '\r')
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                ESP_LOGW(TAG, "Received empty MQTT username via BLE, not storing");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            ESP_LOGI(TAG, "Received MQTT Username via BLE: %s", username);

            nvsManager.writeString("mqtt_username", username);
            return 0;
        }

        /* Handle MQTT Password characteristic */
        if (attr_handle == mqtt_password_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 64) // Max length for password
            {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char password[65] = {0};
            os_mbuf_copydata(ctxt->om, 0, len, password);

            // 检查MQTT密码是否为空或只包含空白字符
            bool isEmpty = true;
            for (int i = 0; i < len; i++)
            {
                if (password[i] != ' ' && password[i] != '\t' && password[i] != '\n' && password[i] != '\r')
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                ESP_LOGW(TAG, "Received empty MQTT password via BLE, not storing");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            ESP_LOGI(TAG, "Received MQTT Password via BLE (length: %d)", len);

            nvsManager.writeString("mqtt_password", password);

            return 0;
        }

        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(TAG,
             "unexpected access operation to MQTT characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

int BleManager::ssid_chr_access(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;
    NVSManager nvsManager("storage");
    nvsManager.init();
    /* Handle access events */
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == connect_status_chr_val_handle)
        {
            // 读取 WiFi 连接状态
            auto &wifi = WiFiManager::getInstance();
            uint8_t connect_status = wifi.isConnected() ? 1 : 0; // 0: 未连接，1: 已连接
            rc = os_mbuf_append(ctxt->om, &connect_status,
                                sizeof(connect_status));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        if (attr_handle == mqtt_status_chr_val_handle)
        {
            // 读取 MQTT 连接状态
            auto &app = MQTTManager::getInstance();
            uint8_t mqtt_status = app.isConnected() ? 1 : 0; // 0: 未连接，1: 已连接
            rc = os_mbuf_append(ctxt->om, &mqtt_status, sizeof(mqtt_status));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        // Read WiFi SSID
        if (attr_handle == ssid_chr_val_handle)
        {
            std::string ssid_str = nvsManager.readString(WIFI_SSID);

            // 如果SSID为空，不发送任何内容
            if (ssid_str.empty())
            {
                ESP_LOGW(TAG, "WiFi SSID is empty, not sending to BLE client");
                goto error;
            }

            ESP_LOGI(TAG, "Read WiFi SSID via BLE：%s", ssid_str.c_str());
            rc = os_mbuf_append(ctxt->om, ssid_str.c_str(), ssid_str.length());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        // Read WiFi Password
        if (attr_handle == password_chr_val_handle)
        {
            std::string password_str = nvsManager.readString(WIFI_PASSWORD);

            // 如果密码为空，不发送任何内容
            if (password_str.empty())
            {
                ESP_LOGW(TAG, "WiFi password is empty, not sending to BLE client");
                goto error;
            }

            ESP_LOGI(TAG, "Read WiFi Password via BLE:%s", password_str.c_str());
            rc = os_mbuf_append(ctxt->om, password_str.c_str(), password_str.length());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        goto error;

    /* Write characteristic event */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGI(TAG, "characteristic write; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        }
        else
        {
            ESP_LOGI(TAG,
                     "characteristic write by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Verify attribute handle */
        if (attr_handle == ssid_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 32)
            { // WiFi SSID 最大 32 字节
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char ssid[33] = {0}; // 多一个 '\0'
            os_mbuf_copydata(ctxt->om, 0, len, ssid);

            // 检查SSID是否为空或只包含空白字符
            bool isEmpty = true;
            for (int i = 0; i < len; i++)
            {
                if (ssid[i] != ' ' && ssid[i] != '\t' && ssid[i] != '\n' && ssid[i] != '\r')
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                ESP_LOGW(TAG, "Received empty WiFi SSID via BLE, not storing");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            ESP_LOGI(TAG, "Received WiFi SSID via BLE: %s", ssid);

            // Store the WiFi SSID in NVS
            bool err = nvsManager.writeString(WIFI_SSID, ssid);
            if (!err)
            {
                ESP_LOGE(TAG, "Failed to store WiFi SSID in NVS: %s", esp_err_to_name(err));
            }

            return 0;
        }
        /* Verify attribute handle */
        if (attr_handle == password_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 32)
            { // WiFi Password 最大 32 字节
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char password[33] = {0}; // 多一个 '\0'
            os_mbuf_copydata(ctxt->om, 0, len, password);

            // 检查密码是否为空或只包含空白字符
            bool isEmpty = true;
            for (int i = 0; i < len; i++)
            {
                if (password[i] != ' ' && password[i] != '\t' && password[i] != '\n' && password[i] != '\r')
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                ESP_LOGW(TAG, "Received empty WiFi password via BLE, not storing");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            ESP_LOGI(TAG, "Received WiFi Password via BLE：%s", password);

            // Store the WiFi password in NVS
            bool err = nvsManager.writeString(WIFI_PASSWORD, password);
            if (!err)
            {
                ESP_LOGE(TAG, "Failed to store WiFi password in NVS: %s", esp_err_to_name(err));
            }

            auto &app = Application::getInstance();
            xEventGroupSetBits(app.event_group, WIFI_CONNECT_BIT);
            return 0;
        }
        if (attr_handle == connect_chr_val_handle)
        {
            // 根据存储的 SSID 和 Password 连接 WiFi
            std::string ssid = nvsManager.readString(WIFI_SSID);
            std::string password = nvsManager.readString(WIFI_PASSWORD);

            // 检查凭据是否为空
            if (ssid.empty() || password.empty())
            {
                ESP_LOGW(TAG, "Cannot connect: WiFi credentials are empty");
                goto error;
            }

            ESP_LOGI(TAG, "Received connect command via BLE, connecting to WiFi...");

            // 触发WiFi连接
            auto &app = Application::getInstance();
            xEventGroupSetBits(app.event_group, WIFI_CONNECT_BIT);

            return 0;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(TAG,
             "unexpected access operation to WiFi SSID characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
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