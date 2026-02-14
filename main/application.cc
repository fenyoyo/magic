#include "application.h"
#include <esp_log.h>
#include "board.h"
#include "mqtt_manager.h"
#include <string>
#include "mpu6050_sensor.h"
#include "oled_display.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "Application"
/** 按钮 GPIO：按下为低电平（接 GND），松开为高电平（内部上拉） */
#define BUTTON_GPIO       GPIO_NUM_4
/** LED GPIO：按下按钮时亮，松开时灭 */
#define LED_GPIO          GPIO_NUM_5
#define GYRO_STREAM_MS   5

/** 陀螺仪 MQTT 发布主题 */
/** publish 失败（队列满）时等待时间，让已排队消息发完 */
#define MQTT_BACKPRESSURE_MS 30


Application::Application()
{
    ESP_LOGI(TAG, "Application init");
}

Application::~Application()
{
}

void Application::onMQTTMessage(const std::string &topic, const std::string &data, int &data_len)
{
    ESP_LOGI(TAG, "Received message on topic: %.*s", (int)topic.length(), topic.data());
    ESP_LOGI(TAG, "Data: %.*s", (int)data.length(), data.data());

    // 处理控制命令
    if (topic == "device/control")
    {
        // handleCommand(data);
    }
    // 处理配置更新
    else if (topic == "device/config")
    {
        ESP_LOGI(TAG, "Config update received");
        // 解析配置JSON等
    }
}

void Application::onMQTTConnection(bool connected)
{
    m_mqtt_connected = connected;

    if (connected)
    {
        ESP_LOGI(TAG, "MQTT Connected!");

        // auto &mqtt = MQTTManager::getInstance();

        // 连接成功后发布设备状态
        // std::string status = m_device_status ? "ON" : "OFF";
        // mqtt.publish("device/status", status, 1, 1);
        // int msg_id = esp_mqtt_client_publish(m_client,
        //                                      m_publish_topic.c_str(),
        //                                      "ESP32 Connected",
        //                                      0, 1, 0);
        // 订阅控制主题
        // mqtt.subscribe("device/control", 0);
    }
    else
    {
        ESP_LOGW(TAG, "MQTT Disconnected!");
        // 可以在这里添加重连逻辑
    }
}

#define OLED_UPDATE_MS 100

/** 实时将陀螺仪/加速度数据显示到 OLED */
static void oled_imu_task(void *arg)
{
    auto &mpu = MPU6050Sensor::getInstance();
    MPU6050Data data;
    for (;;)
    {
        if (mpu.getData(data))
            oled_show_imu(data.gyro_x, data.gyro_y, data.gyro_z,
                          data.accel_x, data.accel_y, data.accel_z);
        vTaskDelay(pdMS_TO_TICKS(OLED_UPDATE_MS));
    }
}

/** 按钮按下时每 5ms 读陀螺仪并通过 MQTT 发送，松开停止；LED 随按钮亮灭 */
static void button_gyro_task(void *arg)
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

    auto &mpu = MPU6050Sensor::getInstance();
    auto &mqtt = MQTTManager::getInstance();
    uint32_t seq = 0;
    char payload[80];

    for (;;)
    {
        if (gpio_get_level(BUTTON_GPIO) == 0)
        {
            seq = 0;
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGI(TAG, "Button pressed, gyro MQTT stream start");
            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                MPU6050Data data;
                if (mpu.getData(data))
                {
                    int len = snprintf(payload, sizeof(payload),
                                       "{\"seq\":%u,\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}",
                                       (unsigned)seq, data.gyro_x, data.gyro_y, data.gyro_z, data.accel_x, data.accel_y, data.accel_z);
                    if (len > 0 && (size_t)len < sizeof(payload) && mqtt.isConnected())
                    {
                        int msg_id = mqtt.publish(CONFIG_MQTT_SUBSCRIBE_TOPIC_GYRO, payload, (size_t)len, 0, 0);
                        if (msg_id >= 0)
                            seq++;
                        else
                            vTaskDelay(pdMS_TO_TICKS(MQTT_BACKPRESSURE_MS));
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(GYRO_STREAM_MS));
            }
            gpio_set_level(LED_GPIO, 0);
            ESP_LOGI(TAG, "Button released, gyro MQTT stream end (total %u)", (unsigned)seq);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Application::onMQTTError(int error_type, void *error_data)
{
    ESP_LOGE(TAG, "MQTT Error occurred: %d", error_type);

    // 根据错误类型进行处理
    switch (error_type)
    {
    case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
        ESP_LOGE(TAG, "Connection refused!");
        break;
    case MQTT_ERROR_TYPE_TCP_TRANSPORT:
        ESP_LOGE(TAG, "TCP transport error!");
        break;
    default:
        ESP_LOGE(TAG, "Unknown error!");
        break;
    }
}

void Application::Start()
{
    // printf("Application started\n");
    ESP_LOGI(TAG, "Application started");

    // 第一步启动wifi
    Board &board = Board::getInstance();
    board.StartNetwork();
    // 第二步启动mqtt

    auto &mqtt = MQTTManager::getInstance();

    mqtt.setMessageCallback([this](const std::string &topic,
                                   const std::string &data,
                                   int data_len)
                            { this->onMQTTMessage(topic, data, data_len); });

    mqtt.setConnectionCallback([this](bool connected)
                               { this->onMQTTConnection(connected); });

    mqtt.setErrorCallback([this](int error_type, void *error_data)
                          { this->onMQTTError(error_type, error_data); });
    mqtt.init();

    // 第三步启动 MPU6050、OLED，并启动按钮任务与 OLED 实时显示
    auto &mpu = MPU6050Sensor::getInstance();
    if (mpu.init())
    {
        if (oled_init())
            xTaskCreate(oled_imu_task, "oled_imu", 2048, nullptr, 4, nullptr);
        xTaskCreate(button_gyro_task, "btn_gyro", 3072, nullptr, 5, nullptr);
    }
    else
    {
        ESP_LOGW(TAG, "MPU6050 init failed, skip sensor data");
    }
}