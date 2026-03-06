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

constexpr int Application::kTensorArenaSize; // 只需要声明，不需要再赋值

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
    char payload[300];
    while (1)
    {
        if (xQueueReceive(xQueueTrans, &pose, portMAX_DELAY))
        {
            // ESP_LOGI(TAG, "pose=%d %d %d", pose.ax, pose.ay, pose.az);
            // ESP_LOGI(TAG, "quat x:%6.2f y:%6.2f z:%6.2f w:%6.2f\n", pose.qx, pose.qy, pose.qz, pose.qw);
            int len;
            len = snprintf(payload, sizeof(payload),
                           "{\"seq\":%u,\"ax\":%d,\"ay\":%d,\"az\":%d,\"gx\":%d,\"gy\":%d,\"gz\":%d ,\"qx\":%6.2f,\"qy\":%6.2f,\"qz\":%6.2f,\"qw\":%f,\"roll\":%f,\"pitch\":%f,\"yaw\":%f,\"rax\":%d,\"ray\":%d,\"raz\":%d,\"wx\":%d,\"wy\":%d,\"wz\":%d}",
                           (unsigned)pose.seq,
                           pose.ax,
                           pose.ay,
                           pose.az,
                           pose.gx,
                           pose.gy,
                           pose.gz, pose.qx, pose.qy, pose.qz, pose.qw, pose.roll, pose.pitch, pose.yaw, pose.rax, pose.ray, pose.raz, pose.wx, pose.wy, pose.wz);

            mqtt.publish(CONFIG_MQTT_SUBSCRIBE_TOPIC_GYRO, payload, (size_t)len, 0, 0);
        }
    }
    vTaskDelete(NULL);
}

// 辅助函数：执行模型推理
void Application::run_inference()
{
    // Application &app = Application::getInstance();

    // 将收集的数据复制到模型输入张量
    for (int i = 0; i < kNumTimeSteps * kNumFeaturesPerStep; i++)
    {
        input->data.f[i] = collected_data[i];
    }

    // 执行推理
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "推理失败");
        return;
    }

    // 获取结果（最大概率）
    float max_score = -1.0f;
    int predicted_class = 0;
    ESP_LOGI(TAG, "输出类别数：%d", output->dims->data[1]);
    for (int i = 0; i < output->dims->data[1]; i++)
    {
        float score = output->data.f[i];
        ESP_LOGI(TAG, "score[%d] = %.4f", i, score);
        if (score > max_score)
        {
            max_score = score;
            predicted_class = i;
        }
    }

    ESP_LOGI(TAG, "预测结果：类别 %d (置信度：%.4f)", predicted_class, max_score);

    // 重置数据收集索引
    collected_data_index = 0;
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
            ESP_LOGI(TAG, "Button pressed, start collecting MPU6050 data for inference");
            mqtt.publish("/device/start", "", 0, 0, 0);

            // 开始收集数据
            Application &app = Application::getInstance();
            app.collecting_data = true;
            app.collected_data_index = 0;

            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
                {
                    mpu.dmpGetAccel(&aa, fifoBuffer);
                    mpu.dmpGetGravity(&gravity, &q);
                    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
                    mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

                    gpio_set_level(LED_GPIO, 1);

                    int16_t ax, ay, az, gx, gy, gz;
                    mpu.getAcceleration(&ax, &ay, &az);
                    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

                    mpu.dmpGetQuaternion(&q, fifoBuffer);
                    mpu.dmpGetEuler(euler, &q);
                    mpu.dmpGetAccel(&aa, fifoBuffer);
                    mpu.dmpGetGravity(&gravity, &q);
                    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
                    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
                    mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

                    POSE_a_g pose;
                    pose.seq = seq;
                    pose.ax = ax;
                    pose.ay = ay;
                    pose.az = az;
                    pose.gx = gx;
                    pose.gy = gy;
                    pose.gz = gz;
                    pose.qx = q.x;
                    pose.qy = q.y;
                    pose.qz = q.z;
                    pose.qw = q.w;
                    pose.roll = ypr[2] * RAD_TO_DEG;
                    pose.pitch = ypr[1] * RAD_TO_DEG;
                    pose.yaw = ypr[0] * RAD_TO_DEG;
                    pose.rax = aaReal.x;
                    pose.ray = aaReal.y;
                    pose.raz = aaReal.z;
                    pose.wx = aaWorld.x;
                    pose.wy = aaWorld.y;
                    pose.wz = aaWorld.z;

                    if (xQueueSend(xQueueTrans, &pose, 100) != pdPASS)
                    {
                        ESP_LOGE(TAG, "xQueueSend fail");
                    }

                    // 如果正在收集数据且未超出缓冲区大小，则保存数据用于推理
                    if (app.collecting_data && app.collected_data_index < app.kNumTimeSteps)
                    {
                        // 存储六轴数据 (acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z)
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 0] = (float)ax / 8192.0f; // 归一化加速度计数据
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 1] = (float)ay / 8192.0f;
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 2] = (float)az / 8192.0f;
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 3] = (float)gx / 131.0f; // 归一化陀螺仪数据
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 4] = (float)gy / 131.0f;
                        app.collected_data[app.collected_data_index * app.kNumFeaturesPerStep + 5] = (float)gz / 131.0f;

                        app.collected_data_index++;

                        // 如果已收集足够的数据，停止收集
                        if (app.collected_data_index >= app.kNumTimeSteps)
                        {
                            ESP_LOGI(TAG, "Collected enough data for inference (%d samples)", app.collected_data_index);
                            app.collecting_data = false;
                        }
                    }
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
            ESP_LOGI(TAG, "Button released, collected %u samples", (unsigned)seq);

            // 按钮释放后，如果收集到了足够的数据，则执行推理
            Application &app_instance = Application::getInstance();
            if (app_instance.collected_data_index > 0)
            {
                ESP_LOGI(TAG, "Executing inference with %d samples", app_instance.collected_data_index);

                // 如果收集的数据不足，用最后的数据填充剩余空间
                if (app_instance.collected_data_index < app_instance.kNumTimeSteps)
                {
                    ESP_LOGW(TAG, "Insufficient data collected (%d/%d), padding with last values",
                             app_instance.collected_data_index, app_instance.kNumTimeSteps);
                    for (int i = app_instance.collected_data_index; i < app_instance.kNumTimeSteps; i++)
                    {
                        // 使用最后一个有效数据点填充
                        for (int j = 0; j < app_instance.kNumFeaturesPerStep; j++)
                        {
                            app_instance.collected_data[i * app_instance.kNumFeaturesPerStep + j] =
                                app_instance.collected_data[(app_instance.collected_data_index - 1) * app_instance.kNumFeaturesPerStep + j];
                        }
                    }
                    app_instance.collected_data_index = app_instance.kNumTimeSteps;
                }

                // 执行推理
                app.run_inference();
            }
            else
            {
                ESP_LOGW(TAG, "No data collected, skipping inference");
            }
        }

        if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
        {
            // 在非数据收集模式下，仍然可以获取数据用于其他用途（如显示）
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

void Application::Start()
{
    // printf("Application started\n");
    ESP_LOGI(TAG, "Application started");
    gpio_set_level(LED_GPIO_R, 1);
    // 第一步启动wifi
    Board &board = Board::getInstance();
    board.StartNetwork();
    board.SetButton();
    // 第二步启动mqtt
    // if (!board.initOLED())
    // {
    //     ESP_LOGE(TAG, "Failed to initialize OLED via Board class");
    // }
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
    xQueueTrans = xQueueCreate(10, sizeof(POSE_a_g));

    configASSERT(xQueueTrans);

    xQueueTransOled = xQueueCreate(10, sizeof(POSE_a));

    configASSERT(xQueueTransOled);
    // Start imu task
    xTaskCreate(&mpu6050, "IMU", 1024 * 8, NULL, 5, NULL);
    xTaskCreate(&mqtt_trans, "MQTT", 1024 * 8, NULL, 5, NULL);

    model = tflite::GetModel(g_person_detect_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        MicroPrintf("Model provided is schema version %d not equal to supported "
                    "version %d.",
                    model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    // ESP_LOGI(TAG, "Model schema version: %lu", tensor_arena);
    // if (tensor_arena == NULL)
    // {
    //     tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // }
    // if (tensor_arena == NULL)
    // {
    //     printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize);
    //     return;
    // }

    // static tflite::MicroMutableOpResolver<5> micro_op_resolver;
    // micro_op_resolver.AddAveragePool2D();
    // micro_op_resolver.AddConv2D();
    // micro_op_resolver.AddDepthwiseConv2D();
    // micro_op_resolver.AddReshape();
    // micro_op_resolver.AddSoftmax();

    static tflite::MicroMutableOpResolver<8> micro_op_resolver;
    micro_op_resolver.AddExpandDims();     // 输入 reshape
    micro_op_resolver.AddConv2D();         // Conv1D 层
    micro_op_resolver.AddAveragePool2D();  // Max/GlobalPooling 层
    micro_op_resolver.AddMaxPool2D();      // 输入 reshape 用
    micro_op_resolver.AddReshape();        // 输入 reshape 用
    micro_op_resolver.AddMean();           // Dense 层
    micro_op_resolver.AddFullyConnected(); // Dense 层
    micro_op_resolver.AddSoftmax();        // 输出 softmax

    // Build an interpreter to run the model with.
    // NOLINTNEXTLINE(runtime-global-variables)
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        MicroPrintf("AllocateTensors() failed");
        return;
    }
    input = interpreter->input(0);
    output = interpreter->output(0);
    // 打印模型信息
    ESP_LOGI(TAG, "模型初始化成功!");
    ESP_LOGI(TAG, "输入形状：");
    for (int i = 0; i < input->dims->size; i++)
    {
        ESP_LOGI(TAG, "  维度 %d: %d", i, input->dims->data[i]);
    }
    ESP_LOGI(TAG, "输出类别数：%d", output->dims->data[1]);
    ESP_LOGI(TAG, "张量竞技场使用：%zu / %d 字节",
             interpreter->arena_used_bytes(), kTensorArenaSize);

    // 初始化数据收集索引
    getInstance().collected_data_index = 0;
    getInstance().collecting_data = false;

    // 启动OLED显示任务（现在由Board类管理）
    // 注意：实际的OLED任务现在在Board类中管理，这里不需要再创建

    // board.getOLED()->display_message("Hello, World!", 16);
    gpio_set_level(LED_GPIO_R, 0);

    // while (1)
    // {
    //     if (gpio_get_level(BUTTON_GPIO_R) == 0)
    //     {
    //         ESP_LOGI(TAG, "Button pressed222222222222");
    //     }
    //     // vTaskDelay(1000);
    // }

    // vTaskDelete(NULL);
}