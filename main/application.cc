#include "application.h"
#include <esp_log.h>
#include "board.h"
#include "mqtt_manager.h"
#include <string>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"

#include "parameter.h"
// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"

// #include "MPU6050.h" // not necessary if using MotionApps include file
#include "MPU6050_6Axis_MotionApps20.h"

#include "ssd1306.h"

#define TAG "Application"

#define GYRO_STREAM_MS 20

/** 陀螺仪 MQTT 发布主题 */
/** publish 失败（队列满）时等待时间，让已排队消息发完 */
#define MQTT_BACKPRESSURE_MS 30
#define OLED_UPDATE_MS 100

#define RAD_TO_DEG (180.0 / M_PI)
#define DEG_TO_RAD 0.0174533

#define I2C_MASTER_SCL_IO 17      /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 18      /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */

MPU6050 mpu;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;        // [w, x, y, z]			quaternion container
VectorInt16 aa;      // [x, y, z]			accel sensor measurements
VectorInt16 gg;      // [x, y, z]			accel sensor measurements
VectorInt16 aaReal;  // [x, y, z]			gravity-free accel sensor measurements
VectorInt16 aaWorld; // [x, y, z]			world-frame accel sensor measurements
VectorFloat gravity; // [x, y, z]			gravity vector
float euler[3];      // [psi, theta, phi]	Euler angle container
float ypr[3];        // [yaw, pitch, roll]	yaw/pitch/roll container and gravity vector

void Application::mqtt_trans(void *pvParameters)
{
    ESP_LOGI(TAG, "mqtt Start");
    auto &mqtt = MQTTManager::getInstance();
    POSE_a_g pose;
    char payload[120];
    while (1)
    {
        if (xQueueReceive(xQueueTrans, &pose, portMAX_DELAY))
        {
            // ESP_LOGI(TAG, "pose=%d %d %d", pose.ax, pose.ay, pose.az);
            int len;
            len = snprintf(payload, sizeof(payload),
                           "{\"seq\":%u,\"ax\":%d,\"ay\":%d,\"az\":%d,\"gx\":%d,\"gy\":%d,\"gz\":%d}",
                           (unsigned)pose.seq,
                           pose.ax,
                           pose.ay,
                           pose.az,
                           pose.gx,
                           pose.gy,
                           pose.gz);
            mqtt.publish(CONFIG_MQTT_SUBSCRIBE_TOPIC_GYRO, payload, (size_t)len, 0, 0);
        }
    }
    vTaskDelete(NULL);
}

// 静态函数用于OLED显示任务
void Application::oled_trans(void *pvParameters)
{
    ESP_LOGI(TAG, "OLED Display Task Started");

    // 等待OLED初始化完成
    vTaskDelay(pdMS_TO_TICKS(100));

    // 确保SSD1306设备已初始化
    if (ssd1306_dev == NULL)
    {
        ESP_LOGE(TAG, "SSD1306 device not initialized, exiting task");
        vTaskDelete(NULL);
        return;
    }

    // 初始化OLED显示
    ssd1306_refresh_gram(ssd1306_dev);
    ssd1306_clear_screen(ssd1306_dev, 0x00);

    char title_str[20] = "GYRO DATA";
    if (ssd1306_dev != NULL)
    {
        ssd1306_draw_string(ssd1306_dev, 10, 0, (const uint8_t *)title_str, 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);
    }

    POSE_a_g pose;
    char buffer[64];
    bool has_received_data = false; // 跟踪是否收到过数据

    while (1)
    {
        if (xQueueReceive(xQueueTransOled, &pose, pdMS_TO_TICKS(50)) == pdTRUE)
        { // 使用较短的超时时间，避免长时间阻塞
            has_received_data = true;

            // 清除屏幕特定区域用于显示数据
            if (ssd1306_dev != NULL)
            {
                ssd1306_clear_screen(ssd1306_dev, 0x00); // 先清除整个屏幕再重新绘制

                // 重新绘制标题
                ssd1306_draw_string(ssd1306_dev, 10, 0, (const uint8_t *)title_str, 16, 1);

                // 显示加速度数据 (ax, ay, az) - 分两行显示，节省空间
                snprintf(buffer, sizeof(buffer), "A:%d,%d", pose.ax, pose.ay);
                ssd1306_draw_string(ssd1306_dev, 0, 20, (const uint8_t *)buffer, 12, 1);

                snprintf(buffer, sizeof(buffer), "Z:%d", pose.az);
                ssd1306_draw_string(ssd1306_dev, 0, 35, (const uint8_t *)buffer, 12, 1);

                // 显示陀螺仪数据 (gx, gy, gz) - 分两行显示
                snprintf(buffer, sizeof(buffer), "G:%d,%d", pose.gx, pose.gy);
                ssd1306_draw_string(ssd1306_dev, 0, 50, (const uint8_t *)buffer, 12, 1);

                snprintf(buffer, sizeof(buffer), "Z:%d", pose.gz);
                ssd1306_draw_string(ssd1306_dev, 64, 50, (const uint8_t *)buffer, 12, 1);

                // 显示序列号
                snprintf(buffer, sizeof(buffer), "#%u", pose.seq);
                ssd1306_draw_string(ssd1306_dev, 110, 0, (const uint8_t *)buffer, 8, 1);

                // 刷新显示
                ssd1306_refresh_gram(ssd1306_dev);
            }
        }
        else
        {
            // 如果队列为空但之前收到过数据，保持最后数据显示一段时间
            if (has_received_data)
            {
                // 在这里可以选择保持最后一次数据显示而不清屏
                // 或者添加一些动态效果表明正在等待新数据
                vTaskDelay(pdMS_TO_TICKS(50)); // 短暂延迟，避免CPU占用过高
            }
            else
            {
                // 如果从未收到数据，显示等待信息
                if (ssd1306_dev != NULL)
                {
                    ssd1306_clear_screen(ssd1306_dev, 0x00);
                    ssd1306_draw_string(ssd1306_dev, 10, 0, (const uint8_t *)"GYRO DATA", 16, 1);
                    ssd1306_draw_string(ssd1306_dev, 15, 35, (const uint8_t *)"WAITING...", 12, 1);
                    ssd1306_refresh_gram(ssd1306_dev);
                }
                vTaskDelay(pdMS_TO_TICKS(100)); // 等待状态下稍微延长延迟
            }
        }
    }

    vTaskDelete(NULL);
}

void Application::mpu6050(void *pvParameters)
{
    // Initialize mpu6050
    mpu.initialize();

    // Get Device ID
    uint8_t buffer[1];
    I2Cdev::readByte(MPU6050_ADDRESS_AD0_LOW, MPU6050_RA_WHO_AM_I, buffer);
    ESP_LOGI(TAG, "getDeviceID=0x%x", buffer[0]);

    // Initialize DMP
    devStatus = mpu.dmpInitialize();

    ESP_LOGI(TAG, "devStatus=%d", devStatus);
    if (devStatus != 0)
    {
        ESP_LOGE(TAG, "DMP Initialization failed [%d]", devStatus);
        while (1)
        {
            vTaskDelay(1);
        }
    }

    // This need to be setup individually
    // supply your own gyro offsets here, scaled for min sensitivity
    // mpu.setXAccelOffset(7188);
    // mpu.setYAccelOffset(6404);
    // mpu.setZAccelOffset(8664);
    // mpu.setXGyroOffset(-88);
    // mpu.setYGyroOffset(68);
    // mpu.setZGyroOffset(20);

    // Calibration Time: generate offsets and calibrate our MPU6050
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.setDMPEnabled(true);
    // TickType_t last_wake_time = xTaskGetTickCount();
    auto &mqtt = MQTTManager::getInstance();
    uint32_t seq = 0;

    while (1)
    {
        if (gpio_get_level(BUTTON_GPIO) == 0)
        {
            seq = 0;
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGI(TAG, "Button pressed, gyro MQTT stream start");
            mqtt.publish("/device/start", "", 0, 0, 0);
            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
                {
                    // float _roll = ypr[2] * RAD_TO_DEG;
                    // float _pitch = ypr[1] * RAD_TO_DEG;
                    // float _yaw = ypr[0] * RAD_TO_DEG;
                    gpio_set_level(LED_GPIO, 1);
                    // Get the Latest packet
                    // getYawPitchRoll();                    // len = snprintf(payload, sizeof(payload),
                    //                "{\"seq\":%u,\"time\":%f,\"_roll\":%.4f,\"_pitch\":%.4f,\"_yaw\":%.4f}",
                    //                (unsigned)seq,
                    //                dt,
                    //                _roll,
                    //                _pitch,
                    //                _yaw);
                    // ESP_LOGI(TAG, "%s", payload);
                    // getWorldAccel();
                    int16_t ax, ay, az, gx, gy, gz;
                    // mpu.getAcceleration(&ax, &ay, &az);
                    // mpu.getRotation(&gx, &gy, &gz);
                    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
                    POSE_a_g pose;
                    pose.seq = seq;
                    pose.ax = ax;
                    pose.ay = ay;
                    pose.az = az;
                    pose.gx = gx;
                    pose.gy = gy;
                    pose.gz = gz;

                    if (xQueueSend(xQueueTrans, &pose, 100) != pdPASS)
                    {
                        ESP_LOGE(TAG, "xQueueSend fail");
                    }
                    if (xQueueSend(xQueueTransOled, &pose, 100) != pdPASS)
                    {
                        ESP_LOGE(TAG, "xQueueSend fail");
                    }
                    // len = snprintf(payload, sizeof(payload),
                    //                "{\"seq\":%u,\"time\":%f,\"x\":%d,\"y\":%d,\"z\":%d}",
                    //                (unsigned)seq,
                    //                dt,
                    //                aaWorld.x,
                    //                aaWorld.y,
                    //                aaWorld.z);
                    // ESP_LOGI(TAG, "%s", payload);
                    // mqtt.publish(CONFIG_MQTT_SUBSCRIBE_TOPIC_GYRO, payload, (size_t)len, 0, 0);
                }
                else
                {
                    ESP_LOGI(TAG, "dmpGetCurrentFIFOPacket fail");
                }
                seq++;
                vTaskDelay(pdMS_TO_TICKS(GYRO_STREAM_MS));
            }
            gpio_set_level(LED_GPIO, 0);
            mqtt.publish("/device/stop", "", 0, 0, 0);
            ESP_LOGI(TAG, "Button released, gyro MQTT stream end (total %u)", (unsigned)seq);
        }

        if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
        {

            // mqtt.publish("/device/start", "", 0, 0, 0);
            // float _roll = ypr[2] * RAD_TO_DEG;
            // float _pitch = ypr[1] * RAD_TO_DEG;
            // float _yaw = ypr[0] * RAD_TO_DEG;

            // // Send UDP packet
            // POSE_t pose;
            // pose.roll = _roll;
            // pose.pitch = _pitch;
            // pose.yaw = _yaw;
            // if (xQueueSend(xQueueTrans, &pose, 100) != pdPASS)
            // {
            //     ESP_LOGE(TAG, "xQueueSend fail");
            // }

            // int len;
            // char payload[120]; // 增加payload大小以容纳预测数据
            // len = snprintf(payload, sizeof(payload),
            //                "{\"seq\":%u,\"_roll\":%.4f,\"_pitch\":%.4f,\"_yaw\":%.4f}",
            //                (unsigned)0,
            //                _roll,
            //                _pitch,
            //                _yaw);
            // mqtt.publish(CONFIG_MQTT_SUBSCRIBE_TOPIC_GYRO, payload, (size_t)len, 0, 0);

            // getQuaternion();
            // getEuler();
            // getRealAccel();
            // getWorldAccel();
        }

        // Best result is to match with DMP refresh rate
        // Its last value in components/MPU6050/MPU6050_6Axis_MotionApps20.h file line 310
        // Now its 0x13, which means DMP is refreshed with 10Hz rate
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Never reach here
    vTaskDelete(NULL);
}

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
QueueHandle_t Application::xQueueTrans = nullptr;
QueueHandle_t Application::xQueueTransOled = nullptr;
ssd1306_handle_t Application::ssd1306_dev = NULL;

void Application::Start()
{
    // printf("Application started\n");
    ESP_LOGI(TAG, "Application started");

    // 第一步启动wifi
    Board &board = Board::getInstance();
    board.StartNetwork();
    board.SetButton();
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
    // Initialize i2c
    I2Cdev::initialize(400000);
    xQueueTrans = xQueueCreate(10, sizeof(POSE_a));

    configASSERT(xQueueTrans);

    xQueueTransOled = xQueueCreate(10, sizeof(POSE_a));

    configASSERT(xQueueTransOled);
    // Start imu task
    xTaskCreate(&mpu6050, "IMU", 1024 * 8, NULL, 5, NULL);
    xTaskCreate(&mqtt_trans, "MQTT", 1024 * 8, NULL, 5, NULL);

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 初始化SSD1306 OLED显示屏
    ssd1306_dev = ssd1306_create(I2C_NUM_1, SSD1306_I2C_ADDRESS);
    if (ssd1306_dev != NULL)
    {
        esp_err_t ret = ssd1306_init(ssd1306_dev);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SSD1306: %d", ret);
            ssd1306_delete(ssd1306_dev);
            ssd1306_dev = NULL;
        }
        else
        {
            ESP_LOGI(TAG, "SSD1306 initialized successfully");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create SSD1306 device handle");
    }

    // 启动OLED显示任务
    xTaskCreate(&oled_trans, "OLED_DISPLAY", 1024 * 4, NULL, 4, NULL);
}