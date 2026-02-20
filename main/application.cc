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

#define TAG "Application"

#define GYRO_STREAM_MS 20

/** 陀螺仪 MQTT 发布主题 */
/** publish 失败（队列满）时等待时间，让已排队消息发完 */
#define MQTT_BACKPRESSURE_MS 30
#define OLED_UPDATE_MS 100

#define RAD_TO_DEG (180.0 / M_PI)
#define DEG_TO_RAD 0.0174533

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
VectorInt16 aaReal;  // [x, y, z]			gravity-free accel sensor measurements
VectorInt16 aaWorld; // [x, y, z]			world-frame accel sensor measurements
VectorFloat gravity; // [x, y, z]			gravity vector
float euler[3];      // [psi, theta, phi]	Euler angle container
float ypr[3];        // [yaw, pitch, roll]	yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = {'$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n'};

// display quaternion values in easy matrix form: w x y z
void getQuaternion()
{
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    printf("quat x:%6.2f y:%6.2f z:%6.2f w:%6.2f\n", q.x, q.y, q.z, q.w);
}

// display Euler angles in degrees
void getEuler()
{
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetEuler(euler, &q);
    printf("euler psi:%6.2f theta:%6.2f phi:%6.2f\n", euler[0] * RAD_TO_DEG, euler[1] * RAD_TO_DEG, euler[2] * RAD_TO_DEG);
}

// display Euler angles in degrees
void getYawPitchRoll()
{
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
#if 0
	float _roll = ypr[2] * RAD_TO_DEG;
	float _pitch = ypr[1] * RAD_TO_DEG;
	float _yaw = ypr[0] * RAD_TO_DEG;
	ESP_LOGI(TAG, "roll:%f pitch:%f yaw:%f",_roll, _pitch, _yaw);
#endif
    // printf("ypr roll:%3.1f pitch:%3.1f yaw:%3.1f\n",ypr[2] * RAD_TO_DEG, ypr[1] * RAD_TO_DEG, ypr[0] * RAD_TO_DEG);
    ESP_LOGI(TAG, "roll:%f pitch:%f yaw:%f", ypr[2] * RAD_TO_DEG, ypr[1] * RAD_TO_DEG, ypr[0] * RAD_TO_DEG);
}

// display real acceleration, adjusted to remove gravity
void getRealAccel()
{
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetAccel(&aa, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
    printf("areal x=%d y:%d z:%d\n", aaReal.x, aaReal.y, aaReal.z);
}

// display initial world-frame acceleration, adjusted to remove gravity
// and rotated based on known orientation from quaternion
void getWorldAccel()
{
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetAccel(&aa, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
    mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
    printf("aworld x:%d y:%d z:%d\n", aaWorld.x, aaWorld.y, aaWorld.z);
}

void Application::mqtt_trans(void *pvParameters)
{
    ESP_LOGI(TAG, "mqtt Start");
    // auto &mqtt = MQTTManager::getInstance();
    // POSE_a pose;
    // while (1)
    // {
    //     if (xQueueReceive(xQueueTrans, &pose, portMAX_DELAY))
    //     {
    //         ESP_LOGI(TAG, "pose=%d %d %d", pose.ax, pose.ay, pose.az);
    //         // sprintf(buffer, "y168.8099yp12.7914pr-11.8401r");
    //         // int buflen = sprintf(buffer, "y%fyp%fpr%fr", pose.yaw, pose.pitch, pose.roll);
    //         // ret = lwip_sendto(fd, buffer, buflen, 0, (struct sockaddr *)&addr, sizeof(addr));
    //         // LWIP_ASSERT("ret == buflen", ret == buflen);
    //         // ESP_LOGD(TAG, "lwip_sendto ret=%d", ret);
    //     }
    //     else
    //     {
    //         ESP_LOGE(TAG, "xQueueReceive fail");
    //         break;
    //     }

    //     // printf("mqtt_trans\n");
    //     // vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    // vTaskDelete(NULL);
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
    mpu.setXAccelOffset(7188);
    mpu.setYAccelOffset(6404);
    mpu.setZAccelOffset(8664);
    mpu.setXGyroOffset(-88);
    mpu.setYGyroOffset(68);
    mpu.setZGyroOffset(20);

    // Calibration Time: generate offsets and calibrate our MPU6050
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.setDMPEnabled(true);
    // TickType_t last_wake_time = xTaskGetTickCount();
    // auto &mqtt = MQTTManager::getInstance();
    uint32_t seq = 0;
    char payload[120];
    while (1)
    {
        if (gpio_get_level(BUTTON_GPIO) == 0)
        {
            seq = 0;
            gpio_set_level(LED_GPIO, 1);
            TickType_t last_wake_time = xTaskGetTickCount(); // 用于计算时间间隔
            ESP_LOGI(TAG, "Button pressed, gyro MQTT stream start");
            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
                {
                    // float _roll = ypr[2] * RAD_TO_DEG;
                    // float _pitch = ypr[1] * RAD_TO_DEG;
                    // float _yaw = ypr[0] * RAD_TO_DEG;

                    seq = 0;
                    gpio_set_level(LED_GPIO, 1);
                    TickType_t current_time = xTaskGetTickCount();
                    float dt = (float)(current_time - last_wake_time) * portTICK_PERIOD_MS;
                    last_wake_time = current_time;
                    // Get the Latest packet
                    // getYawPitchRoll();
                    int len;
                    // len = snprintf(payload, sizeof(payload),
                    //                "{\"seq\":%u,\"time\":%f,\"_roll\":%.4f,\"_pitch\":%.4f,\"_yaw\":%.4f}",
                    //                (unsigned)seq,
                    //                dt,
                    //                _roll,
                    //                _pitch,
                    //                _yaw);
                    // ESP_LOGI(TAG, "%s", payload);
                    getWorldAccel();

                    // POSE_a pose;
                    // pose.ax = aaWorld.x;
                    // pose.ay = aaWorld.y;
                    // pose.az = aaWorld.z;
                    // if (xQueueSend(xQueueTrans, &pose, 100) != pdPASS)
                    // {
                    //     ESP_LOGE(TAG, "xQueueSend fail");
                    // }

                    len = snprintf(payload, sizeof(payload),
                                   "{\"seq\":%u,\"time\":%f,\"x\":%d,\"y\":%d,\"z\":%d}",
                                   (unsigned)seq,
                                   dt,
                                   aaWorld.x,
                                   aaWorld.y,
                                   aaWorld.z);
                    ESP_LOGI(TAG, "%s", payload);
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
            // mqtt.publish("/device/stop", "", 0, 0, 0);
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

    xQueueTrans = xQueueCreate(10, sizeof(POSE_t));
    configASSERT(xQueueTrans);

    // Start imu task
    xTaskCreate(&mpu6050, "IMU", 1024 * 8, NULL, 5, NULL);

    // xTaskCreate(&mqtt_trans, "UDP", 1024 * 3, NULL, 5, NULL);
}