#ifndef _INFERENCE_ENGINE_H_
#define _INFERENCE_ENGINE_H_

#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "person_detect_model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

class InferenceEngine
{
private:
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;

    static constexpr int kTensorArenaSize = 150 * 1024;
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