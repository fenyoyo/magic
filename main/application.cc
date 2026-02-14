#include "application.h"
#include <esp_log.h>
#include "board.h"
#include "mqtt_manager.h"
#include <string>
#include "mpu6050_sensor.h"

#define TAG "Application"

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

        auto &mqtt = MQTTManager::getInstance();

        // 连接成功后发布设备状态
        std::string status = m_device_status ? "ON" : "OFF";
        mqtt.publish("device/status", status, 1, 1);
        // int msg_id = esp_mqtt_client_publish(m_client,
        //                                      m_publish_topic.c_str(),
        //                                      "ESP32 Connected",
        //                                      0, 1, 0);
        // 订阅控制主题
        mqtt.subscribe("device/control", 0);
    }
    else
    {
        ESP_LOGW(TAG, "MQTT Disconnected!");
        // 可以在这里添加重连逻辑
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

    // 第三步启动MPU6050，并读取一秒内的陀螺仪数据
    auto &mpu = MPU6050Sensor::getInstance();
    if (mpu.init())
    {
        MPU6050Data data;
        if (mpu.getData(data))
        {
            ESP_LOGI(TAG, "MPU6050 Accel: X=%.3f Y=%.3f Z=%.3f g", data.accel_x, data.accel_y, data.accel_z);
            ESP_LOGI(TAG, "MPU6050 Gyro:  X=%.2f Y=%.2f Z=%.2f °/s", data.gyro_x, data.gyro_y, data.gyro_z);
            ESP_LOGI(TAG, "MPU6050 Temp:  %.2f °C", data.temperature);
        }

        MPU6050GyroSnapshot gyro1s;
        if (mpu.readGyroForOneSecond(gyro1s))
        {
            ESP_LOGI(TAG, "MPU6050 1s gyro: %d samples", gyro1s.count);
            if (gyro1s.count > 0)
            {
                float sum_x = 0, sum_y = 0, sum_z = 0;
                for (int i = 0; i < gyro1s.count; i++)
                {
                    sum_x += gyro1s.gyro_x[i];
                    sum_y += gyro1s.gyro_y[i];
                    sum_z += gyro1s.gyro_z[i];
                }
                int n = gyro1s.count;
                ESP_LOGI(TAG, "  mean Gyro X=%.2f Y=%.2f Z=%.2f °/s", sum_x / n, sum_y / n, sum_z / n);
                ESP_LOGI(TAG, "  first X=%.2f Y=%.2f Z=%.2f  last X=%.2f Y=%.2f Z=%.2f °/s",
                        gyro1s.gyro_x[0], gyro1s.gyro_y[0], gyro1s.gyro_z[0],
                        gyro1s.gyro_x[n - 1], gyro1s.gyro_y[n - 1], gyro1s.gyro_z[n - 1]);
            }
        }
        else
        {
            ESP_LOGW(TAG, "MPU6050 readGyroForOneSecond failed");
        }
    }
    else
    {
        ESP_LOGW(TAG, "MPU6050 init failed, skip sensor data");
    }
}