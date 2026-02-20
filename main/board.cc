#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "wifi_manager.h"
#include "driver/gpio.h"

/** 按钮 GPIO：按下为低电平（接 GND），松开为高电平（内部上拉） */
#define BUTTON_GPIO GPIO_NUM_4
/** LED GPIO：按下按钮时亮，松开时灭 */
#define LED_GPIO GPIO_NUM_5

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

void Board::SetButton()
{
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << BUTTON_GPIO);
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
}
