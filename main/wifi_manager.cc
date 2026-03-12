#include "wifi_manager.h"
#include "NVSManager.h"
#include "public.h"

#define TAG "WiFiManager"
EventGroupHandle_t WiFiManager::s_wifi_event_group = nullptr;
int WiFiManager::s_retry_num = 0;
void (*WiFiManager::s_connection_callback)(bool success) = nullptr;

WiFiManager::WiFiManager() : m_is_connected(false)
{
    memset(&m_ip_addr, 0, sizeof(m_ip_addr));
}

WiFiManager::~WiFiManager()
{
    disconnect();
    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }
}

void WiFiManager::eventHandler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        WiFiManager &instance = WiFiManager::getInstance();

        if (s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP, retry count: %d", s_retry_num);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECT_FAIL_BIT);
            instance.m_is_connected = false;

            if (s_connection_callback)
            {
                s_connection_callback(false);
            }
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        WiFiManager &instance = WiFiManager::getInstance();
        instance.m_is_connected = true;
        instance.m_ip_addr = event->ip_info.ip;

        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_connection_callback)
        {
            s_connection_callback(true);
        }
    }
}

void WiFiManager::initSTA()
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &eventHandler,
                                                        nullptr,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &eventHandler,
                                                        nullptr,
                                                        &instance_got_ip));

    // WiFi配置
    NVSManager nvsManager("storage");
    nvsManager.init();
    std::string ssid = nvsManager.readString(WIFI_SSID);
    std::string password = nvsManager.readString(WIFI_PASSWORD);
    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;
    wifi_config.sta.sae_h2e_identifier[0] = '\0';
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HASH_TO_ELEMENT;
    strlcpy((char *)wifi_config.sta.sae_h2e_identifier, CONFIG_ESP_WIFI_PW_ID,
            sizeof(wifi_config.sta.sae_h2e_identifier));
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    strlcpy((char *)wifi_config.sta.sae_h2e_identifier, CONFIG_ESP_WIFI_PW_ID,
            sizeof(wifi_config.sta.sae_h2e_identifier));
#endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

bool WiFiManager::connect()
{
    initSTA();

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_CONNECT_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        m_is_connected = true;
        return true;
    }
    else if (bits & WIFI_CONNECT_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        m_is_connected = false;
        return false;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        m_is_connected = false;
        return false;
    }
}

void WiFiManager::disconnect()
{
    if (m_is_connected)
    {
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();
        m_is_connected = false;
    }
}

bool WiFiManager::isConnected()
{
    return m_is_connected;
}

esp_ip4_addr_t WiFiManager::getIPAddress()
{
    return m_ip_addr;
}

void WiFiManager::setConnectionCallback(void (*callback)(bool success))
{
    s_connection_callback = callback;
}