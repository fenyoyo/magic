#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_mac.h"

#include "wifi_manager.h"
#include "driver/gpio.h"
#include <string>
#include <cstdio>

#define TAG "Board"
Board::Board()
{
    ESP_LOGI(TAG, "Board init");
    m_oled = nullptr;
    getDeviceId();
}

Board::~Board()
{
    if (m_oled)
    {
        delete m_oled;
        m_oled = nullptr;
    }
}

void Board::StartNetwork()
{
    ESP_LOGI(TAG, "StartNetwork");
    // 获取WiFi管理器单例
    WiFiManager &wifi = WiFiManager::getInstance();

    // 设置连接回调
    wifi.setConnectionCallback([](bool success)
                               {
        if (success)
        {
            ESP_LOGI(TAG, "Connected to WiFi");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect to WiFi");
        } });

    // 连接WiFi（异步方式，通过回调获取结果）
    wifi.connect();
}

void Board::SetButton()
{
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    io.pin_bit_mask = (1ULL << BUTTON_GPIO_R);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    io.pin_bit_mask = (1ULL << LED_GPIO);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level(LED_GPIO, 0);

    io.pin_bit_mask = (1ULL << LED_GPIO_R);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level(LED_GPIO_R, 0);
}

bool Board::initOLED()
{
    if (m_oled == nullptr)
    {
        m_oled = new OLED();
        if (!m_oled->initialize())
        {
            ESP_LOGE(TAG, "Failed to initialize OLED");
            delete m_oled;
            m_oled = nullptr;
            return false;
        }
        ESP_LOGI(TAG, "OLED initialized successfully");
    }
    return true;
}

std::string Board::getDeviceId()
{
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
        return "";
    }

    char deviceId[18]; // Format: XX:XX:XX:XX:XX:XX + null terminator
    snprintf(deviceId, sizeof(deviceId), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Device ID: %s", deviceId);
    return std::string(deviceId);
}
