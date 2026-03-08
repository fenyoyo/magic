/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

/* Private function declarations */
static int ssid_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static int mqtt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

/* NVS storage helper functions */
static esp_err_t store_wifi_ssid(const char *ssid);
static esp_err_t store_wifi_password(const char *password);
static esp_err_t get_wifi_ssid(char *ssid, size_t *length);
static esp_err_t get_wifi_password(char *password, size_t *length);
static esp_err_t store_mqtt_addr(const char *server_addr);
static esp_err_t store_mqtt_username(const char *username);
static esp_err_t store_mqtt_password(const char *password);
static esp_err_t get_mqtt_addr(char *server_addr, size_t *length);
static esp_err_t get_mqtt_username(char *username, size_t *length);
static esp_err_t get_mqtt_password(char *password, size_t *length);

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

/* MQTT Configuration service */
static const ble_uuid16_t mqtt_config_svc_uuid = BLE_UUID16_INIT(0x1889); // Custom UUID for MQTT Configuration Service
static uint16_t mqtt_addr_chr_val_handle;
static const ble_uuid16_t mqtt_addr_chr_uuid = BLE_UUID16_INIT(0x2B01); // Custom UUID for MQTT Server Address
static uint16_t mqtt_username_chr_val_handle;
static const ble_uuid16_t mqtt_username_chr_uuid = BLE_UUID16_INIT(0x2B02); // Custom UUID for MQTT Username
static uint16_t mqtt_password_chr_val_handle;
static const ble_uuid16_t mqtt_password_chr_uuid = BLE_UUID16_INIT(0x2B03); // Custom UUID for MQTT Password
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

        // Read WiFi SSID
        if (attr_handle == ssid_chr_val_handle)
        {
            char ssid[33];
            size_t ssid_len = sizeof(ssid);

            esp_err_t err = get_wifi_ssid(ssid, &ssid_len);
            if (err != ESP_OK)
            {
                // Return empty string if not found
                ssid[0] = '\0';
                ssid_len = 0;
            }

            rc = os_mbuf_append(ctxt->om, ssid, ssid_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        // Read WiFi Password
        if (attr_handle == password_chr_val_handle)
        {
            char password[33];
            size_t password_len = sizeof(password);

            esp_err_t err = get_wifi_password(password, &password_len);
            if (err != ESP_OK)
            {
                // Return empty string if not found
                password[0] = '\0';
                password_len = 0;
            }

            rc = os_mbuf_append(ctxt->om, password, password_len);
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

            // Store the WiFi SSID in NVS
            esp_err_t err = store_wifi_ssid(ssid);
            if (err != ESP_OK)
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

            ESP_LOGI(TAG, "Received WiFi Password via BLE");

            // Store the WiFi password in NVS
            esp_err_t err = store_wifi_password(password);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to store WiFi password in NVS: %s", esp_err_to_name(err));
            }

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
 *  MQTT Configuration characteristic access callback
 *      - Handles read/write operations for MQTT configuration
 */
static int mqtt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Handle MQTT Server Address characteristic */
        if (attr_handle == mqtt_addr_chr_val_handle)
        {
            char server_addr[256];
            size_t addr_len = sizeof(server_addr);

            esp_err_t err = get_mqtt_addr(server_addr, &addr_len);
            if (err != ESP_OK)
            {
                // Return empty string if not found
                server_addr[0] = '\0';
                addr_len = 0;
            }

            rc = os_mbuf_append(ctxt->om, server_addr, addr_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        /* Handle MQTT Username characteristic */
        if (attr_handle == mqtt_username_chr_val_handle)
        {
            char username[65];
            size_t username_len = sizeof(username);

            esp_err_t err = get_mqtt_username(username, &username_len);
            if (err != ESP_OK)
            {
                // Return empty string if not found
                username[0] = '\0';
                username_len = 0;
            }

            rc = os_mbuf_append(ctxt->om, username, username_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        /* Handle MQTT Password characteristic */
        if (attr_handle == mqtt_password_chr_val_handle)
        {
            char password[65];
            size_t password_len = sizeof(password);

            esp_err_t err = get_mqtt_password(password, &password_len);
            if (err != ESP_OK)
            {
                // Return empty string if not found
                password[0] = '\0';
                password_len = 0;
            }

            rc = os_mbuf_append(ctxt->om, password, password_len);
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

            ESP_LOGI(TAG, "Received MQTT Server Address via BLE: %s", server_addr);

            // Store the MQTT server address in NVS
            esp_err_t err = store_mqtt_addr(server_addr);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to store MQTT server address in NVS: %s", esp_err_to_name(err));
            }

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

            ESP_LOGI(TAG, "Received MQTT Username via BLE: %s", username);

            // Store the MQTT username in NVS
            esp_err_t err = store_mqtt_username(username);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to store MQTT username in NVS: %s", esp_err_to_name(err));
            }

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

            ESP_LOGI(TAG, "Received MQTT Password via BLE (length: %d)", len);

            // Store the MQTT password in NVS
            esp_err_t err = store_mqtt_password(password);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to store MQTT password in NVS: %s", esp_err_to_name(err));
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
             "unexpected access operation to MQTT characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

/*
 *  NVS storage helper functions
 */
// 通用的NVS字符串存储函数
static esp_err_t nvs_store_string(const char *key, const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

// 通用的NVS字符串读取函数
static esp_err_t nvs_read_string(const char *key, char *value, size_t *length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_get_str(nvs_handle, key, value, length);
    nvs_close(nvs_handle);
    return err;
}

// WiFi配置存储和读取函数
static esp_err_t store_wifi_ssid(const char *ssid)
{
    return nvs_store_string("wifi_ssid", ssid);
}

static esp_err_t store_wifi_password(const char *password)
{
    return nvs_store_string("wifi_password", password);
}

static esp_err_t get_wifi_ssid(char *ssid, size_t *length)
{
    return nvs_read_string("wifi_ssid", ssid, length);
}

static esp_err_t get_wifi_password(char *password, size_t *length)
{
    return nvs_read_string("wifi_password", password, length);
}

// MQTT配置存储和读取函数
static esp_err_t store_mqtt_addr(const char *server_addr)
{
    return nvs_store_string("mqtt_addr", server_addr);
}

static esp_err_t store_mqtt_username(const char *username)
{
    return nvs_store_string("mqtt_username", username);
}

static esp_err_t store_mqtt_password(const char *password)
{
    return nvs_store_string("mqtt_password", password);
}

static esp_err_t get_mqtt_addr(char *server_addr, size_t *length)
{
    return nvs_read_string("mqtt_addr", server_addr, length);
}

static esp_err_t get_mqtt_username(char *username, size_t *length)
{
    return nvs_read_string("mqtt_username", username, length);
}

static esp_err_t get_mqtt_password(char *password, size_t *length)
{
    return nvs_read_string("mqtt_password", password, length);
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
