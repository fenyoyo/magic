#ifndef PUBLIC_H
#define PUBLIC_H

// #include "nvs.h"
// #include "nvs_flash.h"

#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"
#define MQTT_ADDR "mqtt_addr"
#define MQTT_USERNAME "mqtt_username"
#define MQTT_PASSWORD "mqtt_password"
#define MQTT_PORT "mqtt_port"

static constexpr EventBits_t WIFI_CONNECT_BIT = (1 << 0);
static constexpr EventBits_t WIFI_CONNECTED_BIT = (1 << 1);
static constexpr EventBits_t WIFI_CONNECT_FAIL_BIT = (1 << 2);

static constexpr EventBits_t MQTT_CONNECT_BIT = (1 << 3);
static constexpr EventBits_t MQTT_CONNECTED_BIT = (1 << 4);
static constexpr EventBits_t MQTT_CONNECT_FAIL_BIT = (1 << 5);

// /*
//  *  NVS storage helper functions
//  */
// // 通用的NVS字符串存储函数
// static esp_err_t nvs_store_string(const char *key, const char *value)
// {
//     nvs_handle_t nvs_handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
//     if (err != ESP_OK)
//     {
//         return err;
//     }

//     err = nvs_set_str(nvs_handle, key, value);
//     if (err != ESP_OK)
//     {
//         nvs_close(nvs_handle);
//         return err;
//     }

//     err = nvs_commit(nvs_handle);
//     nvs_close(nvs_handle);
//     return err;
// }

// // 通用的NVS字符串读取函数
// static esp_err_t nvs_read_string(const char *key, char *value, size_t *length)
// {
//     nvs_handle_t nvs_handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
//     if (err != ESP_OK)
//     {
//         return err;
//     }

//     err = nvs_get_str(nvs_handle, key, value, length);
//     nvs_close(nvs_handle);
//     return err;
// }

#endif