#include "application.h"
#include <esp_log.h>
#include "board.h"
#include "mqtt_manager.h"
#include <string>
#include "mpu6050_sensor.h"
#include "sliding_average_filter.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 定义USE_KALMAN_FILTER宏以启用卡尔曼滤波器
// #define USE_KALMAN_FILTER 1
#include "mpu6050_kalman_filter.h"

// 包含陀螺仪预测器
#include "gyro_predictor.h"

#define TAG "Application"
/** 按钮 GPIO：按下为低电平（接 GND），松开为高电平（内部上拉） */
#define BUTTON_GPIO GPIO_NUM_4
/** LED GPIO：按下按钮时亮，松开时灭 */
#define LED_GPIO GPIO_NUM_5
#define GYRO_STREAM_MS 5

/** 陀螺仪 MQTT 发布主题 */
/** publish 失败（队列满）时等待时间，让已排队消息发完 */
#define MQTT_BACKPRESSURE_MS 30
#define OLED_UPDATE_MS 100

// 滑动平均滤波器窗口大小配置
#ifndef CONFIG_MPU6050_FILTER_WINDOW_SIZE
#define CONFIG_MPU6050_FILTER_WINDOW_SIZE 5
#endif

Application::Application()
{
    ESP_LOGI(TAG, "Application init");

    // 初始化陀螺仪预测器
    m_gyro_predictor = new GyroPredictor();
    if (m_gyro_predictor->Init())
    {
        ESP_LOGI(TAG, "Gyro predictor initialized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize gyro predictor");
        delete m_gyro_predictor;
        m_gyro_predictor = nullptr;
    }
}

Application::~Application()
{
    // 清理陀螺仪预测器
    if (m_gyro_predictor)
    {
        delete m_gyro_predictor;
        m_gyro_predictor = nullptr;
    }
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

/** 按钮按下时每 5ms 读陀螺仪并通过 MQTT 发送，松开停止；LED 随按钮亮灭 */
void Application::button_gyro_task(void *arg)
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
    char payload[120]; // 增加payload大小以容纳预测数据

    // 获取当前应用实例
    Application *app_instance = static_cast<Application *>(arg);

    // 选择滤波器类型：可以选择使用滑动平均滤波或卡尔曼滤波
    const size_t FILTER_WINDOW_SIZE = CONFIG_MPU6050_FILTER_WINDOW_SIZE;          // 平均最近N个数据点
    MPU6050DataFilter data_filter(FILTER_WINDOW_SIZE, FilterType::KALMAN_FILTER); // 使用卡尔曼滤波

    // 如果使用卡尔曼滤波，可以设置特定的噪声参数
    if (data_filter.getFilterType() == FilterType::KALMAN_FILTER)
    {
        data_filter.setKalmanParameters(0.01f, 0.02f); // 陀螺仪噪声, 加速度计噪声
    }

    for (;;)
    {
        if (gpio_get_level(BUTTON_GPIO) == 0)
        {
            seq = 0;
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGI(TAG, "Button pressed, gyro MQTT stream start");
            mqtt.publish("/device/start", "", 0, 0, 0);

            // 重置滤波器，开始新的数据流
            data_filter.reset();

            TickType_t last_wake_time = xTaskGetTickCount(); // 用于计算时间间隔

            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                MPU6050Data raw_data;
                if (mpu.getData(raw_data))
                {
                    // 计算时间间隔（秒）用于卡尔曼滤波
                    TickType_t current_time = xTaskGetTickCount();
                    float dt = (float)(current_time - last_wake_time) * portTICK_PERIOD_MS / 1000.0f;
                    last_wake_time = current_time;

                    // 应用所选滤波器（滑动平均或卡尔曼）
                    MPU6050Data filtered_data = data_filter.filterData(raw_data, dt);

                    // 如果陀螺仪预测器可用，执行预测
                    float prediction_result[1] = {0.0f}; // 假设模型输出单个值

                    // 创建包含原始数据和预测结果的payload
                    int len;

                    len = snprintf(payload, sizeof(payload),
                                   "{\"seq\":%u,\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}",
                                   (unsigned)seq,
                                   filtered_data.gyro_x,
                                   filtered_data.gyro_y,
                                   filtered_data.gyro_z,
                                   filtered_data.accel_x,
                                   filtered_data.accel_y,
                                   filtered_data.accel_z);

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
            mqtt.publish("/device/stop", "", 0, 0, 0);
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

    // 第三步启动 MPU6050
    auto &mpu = MPU6050Sensor::getInstance();
    if (mpu.init())
    {
        ESP_LOGI(TAG, "MPU6050 initialized successfully with sliding average filter (window size: %d)",
                 CONFIG_MPU6050_FILTER_WINDOW_SIZE);
        xTaskCreate(button_gyro_task, "btn_gyro", 3072, this, 5, &m_button_gyro_task_handle);
    }
    else
    {
        ESP_LOGW(TAG, "MPU6050 init failed, skip sensor data");
    }
}