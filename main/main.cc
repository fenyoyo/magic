/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "application.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "ble_manager.h"
#include "led_service.h"

extern "C" void app_main(void)
{
    // Initialize BLE using BleManager
    BleManager &ble = BleManager::getInstance();
    esp_err_t ret = ble.init();
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "failed to initialize BLE, error code: %d", ret);
        return;
    }

    // ESP_LOGI("MAIN", "Starting application...");

    // 启动程序
    Application::getInstance().Start();

    // 保持运行
    // while (1)
    // {
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
}
