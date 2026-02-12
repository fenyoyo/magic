#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "wifi_manager.h"

#define TAG "Board"
Board::Board()
{
    ESP_LOGI(TAG, "Board init");
}

Board::~Board()
{
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
