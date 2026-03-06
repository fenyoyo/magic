#ifndef _INFERENCE_ENGINE_H_
#define _INFERENCE_ENGINE_H_

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

// 手势枚举
enum class Gesture : uint8_t
{
    LEFT_RIGHT = 0,   // 左右滑动
    LETTER_ALPHA = 1, // 字母 Alpha
    LETTER_M = 2,     // 字母 M
    LETTER_R = 3,     // 字母 R
    LETTER_W = 4,     // 字母 W
    LIGHTNING = 5,    // 闪电
    RIGHT_LEFT = 6,   // 右左滑动
    TRIANGLE = 7,     // 三角形
    UNKNOWN = 255
};

// 手势名称
inline const char *get_gesture_name(Gesture gesture)
{
    switch (gesture)
    {
    case Gesture::LEFT_RIGHT:
        return "左右滑动";
    case Gesture::LETTER_ALPHA:
        return "字母 Alpha";
    case Gesture::LETTER_M:
        return "字母 M";
    case Gesture::LETTER_R:
        return "字母 R";
    case Gesture::LETTER_W:
        return "字母 W";
    case Gesture::LIGHTNING:
        return "闪电";
    case Gesture::RIGHT_LEFT:
        return "右左滑动";
    case Gesture::TRIANGLE:
        return "三角形";
    default:
        return "未知";
    }
}

class InferenceEngine
{
private:
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;

    static constexpr int kTensorArenaSize = 200 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    // 预处理函数
    float *preprocess(float *raw_data, int len);

public:
    InferenceEngine();
    ~InferenceEngine();

    bool initialize();
    void run_normalized_inference(float *collected_data, int collected_data_index, int kNumTimeSteps, int kNumFeaturesPerStep);

    // 获取推理结果
    int get_predicted_class() const { return predicted_class_; }
    float get_confidence() const { return confidence_; }
    const float *get_all_scores() const { return all_scores_; }
    int get_num_classes() const { return num_classes_; }

private:
    int predicted_class_;
    float confidence_;
    float all_scores_[32]; // 假设最多有32个类别
    int num_classes_;
};

#endif // _INFERENCE_ENGINE_H_