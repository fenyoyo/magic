#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference_engine.h"
#include "time_series_normalizer.h"
#include "freertos/event_groups.h"
// 定义推理结果的MQTT主题
#ifndef CONFIG_MQTT_INFERENCE_RESULT_TOPIC
#define CONFIG_MQTT_INFERENCE_RESULT_TOPIC "/device/inference/result"
#endif

class Application
{
private:
    /* data */
    Application();
    ~Application();
    static QueueHandle_t xQueueTrans;
    static QueueHandle_t xQueueTransOled;
    bool m_mqtt_connected;
    static void mpu6050(void *arg);
    static void mqtt_trans(void *arg);
    static void wifi_task(void *arg);
    float *preprocess(float *raw_data, int len)
    {
        static float processed[600]; // 100*6

        // 标准化 (x - mean) / scale
        for (int i = 0; i < len; i++)
        {
            for (int j = 0; j < 6; j++)
            {
                processed[i * 6 + j] = (raw_data[i * 6 + j] - scaler_mean[j]) / scaler_scale[j];
            }
        }
        return processed;
    }

public:
    static Application &getInstance()
    {
        static Application instance;
        return instance;
    };
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void Start();

    void onMQTTMessage(const std::string &topic, const std::string &data, int &data_len);
    void onMQTTConnection(bool connected);
    void onMQTTError(int error_type, void *error_data);
    void publish_inference_result_with_all_scores(int predicted_class, float confidence, float *all_scores, int num_classes);

    InferenceEngine inference_engine;

    // 添加用于存储MPU6050数据的缓冲区
    static constexpr int kNumTimeSteps = 100;                           // 模型期望的时间步数
    static constexpr int kMaxCollectedTimeSteps = 200;                  // 最大可收集的时间步数
    static constexpr int kNumFeaturesPerStep = 6;                       // 每个时间步的特征数 (acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z)
    float collected_data[kMaxCollectedTimeSteps * kNumFeaturesPerStep]; // 增加缓冲区大小以容纳最多200个数据点
    int collected_data_index = 0;
    bool collecting_data = false;

    EventGroupHandle_t event_group;

    static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
    static constexpr EventBits_t WIFI_FAIL_BIT = BIT1;
    static constexpr EventBits_t WIFI_MQTT_CONNECTED_BIT = BIT2;
};

#endif // _APPLICATION_H_