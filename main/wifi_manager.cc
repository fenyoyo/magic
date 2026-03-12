#include "wifi_manager.h"
#include "NVSManager.h"
#include "public.h"
#include "application.h"

#define TAG "WiFiManager"
EventGroupHandle_t WiFiManager::s_wifi_event_group = nullptr;
int WiFiManager::s_retry_num = 0;
bool WiFiManager::s_initialized = false; // 添加静态变量跟踪初始化状态

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
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        WiFiManager &instance = WiFiManager::getInstance();
        auto &app = Application::getInstance();
        if (s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP, retry count: %d", s_retry_num);
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect to the AP after %d retries", MAXIMUM_RETRY);
            xEventGroupSetBits(app.event_group, WIFI_CONNECT_FAIL_BIT);
            instance.m_is_connected = false;
            s_retry_num = 0; // 重置重试计数器
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
        auto &app = Application::getInstance();
        s_retry_num = 0; // 连接成功时重置重试计数器
        xEventGroupSetBits(app.event_group, WIFI_CONNECTED_BIT);
    }
}

void WiFiManager::initSTA()
{
    // 只有在未初始化的情况下才初始化网络接口和事件循环
    if (!s_initialized) {
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
        
        s_initialized = true;
    }

    // 重新加载WiFi配置
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
    // 如果已经连接，先断开之前的连接
    if (m_is_connected) {
        disconnect();
    }
    
    // 检查凭据是否有效
    NVSManager nvsManager("storage");
    nvsManager.init();
    std::string ssid = nvsManager.readString(WIFI_SSID);
    std::string password = nvsManager.readString(WIFI_PASSWORD);
    
    // 如果凭据为空，则不尝试连接
    if (ssid.empty() || password.empty()) {
        ESP_LOGW(TAG, "WiFi credentials are empty, skipping connection attempt");
        return false;
    }
    
    initSTA();

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return true;
}

void WiFiManager::disconnect()
{
    if (m_is_connected)
    {
        esp_wifi_disconnect();
        // 不要停止或反初始化WiFi，只是断开连接
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
