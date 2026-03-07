/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"

/* Private function declarations */
static int ssid_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

/* Automation IO service */
static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
static uint16_t ssid_chr_val_handle;
static const ble_uuid16_t ssid_chr_uuid = BLE_UUID16_INIT(0x2A31);
static uint16_t password_chr_val_handle;
static const ble_uuid16_t password_chr_uuid = BLE_UUID16_INIT(0x2A32);
static uint16_t connect_chr_val_handle;
static const ble_uuid16_t connect_chr_uuid = BLE_UUID16_INIT(0x2A33);

static uint16_t connect_status_chr_val_handle;
static const ble_uuid16_t connect_status_chr_uuid = BLE_UUID16_INIT(0x2A21);
static uint16_t mqtt_status_chr_val_handle;
static const ble_uuid16_t mqtt_status_chr_uuid = BLE_UUID16_INIT(0x2A22);
/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {

    /* Automation IO service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &auto_io_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){/* WiFi SSID characteristic */
                                        {.uuid = &ssid_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE,
                                         .val_handle = &ssid_chr_val_handle},
                                        {.uuid = &password_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE,
                                         .val_handle = &password_chr_val_handle},
                                        {.uuid = &connect_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_WRITE,
                                         .val_handle = &connect_chr_val_handle},
                                        {.uuid = &connect_status_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_READ,
                                         .val_handle = &connect_status_chr_val_handle},
                                        {.uuid = &mqtt_status_chr_uuid.u,
                                         .access_cb = ssid_chr_access,
                                         .flags = BLE_GATT_CHR_F_READ,
                                         .val_handle = &mqtt_status_chr_val_handle},
                                        {0}},
    },

    {
        0, /* No more services. */
    },
};

static int ssid_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    /* Note: WiFi SSID characteristic is write only */
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == connect_status_chr_val_handle)
        {
            // TODO 读取 WiFi 连接状态
            uint8_t connect_status = 1; // 0: 未连接，1: 已连接
            rc = os_mbuf_append(ctxt->om, &connect_status,
                                sizeof(connect_status));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        if (attr_handle == mqtt_status_chr_val_handle)
        {
            // TODO 读取 MQTT 连接状态
            uint8_t mqtt_status = 1; // 0: 未连接，1: 已连接
            rc = os_mbuf_append(ctxt->om, &mqtt_status, sizeof(mqtt_status));
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

            ESP_LOGI(TAG, "Received WiFi SSID via BLE: %s", ssid);

            return 0;
        }
        /* Verify attribute handle */
        if (attr_handle == password_chr_val_handle)
        {
            int len = ctxt->om->om_len;
            if (len == 0 || len > 32)
            { // WiFi SSID 最大 32 字节
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            char password[33] = {0}; // 多一个 '\0'
            os_mbuf_copydata(ctxt->om, 0, len, password);

            ESP_LOGI(TAG, "Received WiFi Password via BLE: %s", password);

            return 0;
        }
        if (attr_handle == connect_chr_val_handle)
        {
            // TODO 根据接收到的 SSID 和 Password 连接 WiFi
            ESP_LOGI(TAG, "Received connect command via BLE, connecting to WiFi...");

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

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op)
    {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    }
    else
    {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void)
{
    /* Local variables */
    int rc;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0)
    {
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0)
    {
        return rc;
    }

    return 0;
}
