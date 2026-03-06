#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"
#include "person_detect_model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "time_series_normalizer.h"

// 定义推理结果的MQTT主题
#ifndef CONFIG_MQTT_INFERENCE_RESULT_TOPIC
#define CONFIG_MQTT_INFERENCE_RESULT_TOPIC "/device/inference/result"
#endif

// 手势枚举
enum class Gesture : uint8_t {
    LEFT_RIGHT = 0,     // 左右滑动
    LETTER_ALPHA = 1,   // 字母 Alpha
    LETTER_M = 2,       // 字母 M
    LETTER_R = 3,       // 字母 R
    LETTER_W = 4,       // 字母 W
    LIGHTNING = 5,      // 闪电
    RIGHT_LEFT = 6,     // 右左滑动
    TRIANGLE = 7,       // 三角形
    UNKNOWN = 255
};

// 手势名称
inline const char* get_gesture_name(Gesture gesture) {
    switch (gesture) {
        case Gesture::LEFT_RIGHT:    return "左右滑动";
        case Gesture::LETTER_ALPHA:  return "字母 Alpha";
        case Gesture::LETTER_M:      return "字母 M";
        case Gesture::LETTER_R:      return "字母 R";
        case Gesture::LETTER_W:      return "字母 W";
        case Gesture::LIGHTNING:     return "闪电";
        case Gesture::RIGHT_LEFT:    return "右左滑动";
        case Gesture::TRIANGLE:      return "三角形";
        default:                     return "未知";
    }
}

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
    void run_inference();
    void run_normalized_inference();
    void publish_inference_result(int predicted_class, float confidence);
    void publish_inference_result_with_all_scores(int predicted_class, float confidence, float* all_scores, int num_classes);

    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    int inference_count = 0;

    static constexpr int kTensorArenaSize = 200 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    // 添加用于存储MPU6050数据的缓冲区
    static constexpr int kNumTimeSteps = 100;     // 模型期望的时间步数
    static constexpr int kNumFeaturesPerStep = 6; // 每个时间步的特征数 (acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z)
    float collected_data[kNumTimeSteps * kNumFeaturesPerStep];
    int collected_data_index = 0;
    bool collecting_data = false;
};

#endif // _APPLICATION_H_