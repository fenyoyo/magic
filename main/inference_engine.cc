#include "inference_engine.h"
#include <esp_log.h>
#include "time_series_normalizer.h"
#include <vector>
#include "application.h" // 包含手势枚举定义

#define TAG "InferenceEngine"

InferenceEngine::InferenceEngine()
{
    predicted_class_ = 0;
    confidence_ = 0.0f;
    num_classes_ = 0;
}

InferenceEngine::~InferenceEngine()
{
    // 清理资源（如果需要）
}

bool InferenceEngine::initialize()
{
    model = tflite::GetModel(person_detect_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        MicroPrintf("Model provided is schema version %d not equal to supported "
                    "version %d.",
                    model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    static tflite::MicroMutableOpResolver<10> micro_op_resolver;
    micro_op_resolver.AddExpandDims();     // 输入 reshape
    micro_op_resolver.AddConv2D();         // Conv1D 层
    micro_op_resolver.AddAveragePool2D();  // Max/GlobalPooling 层
    micro_op_resolver.AddMaxPool2D();      // 输入 reshape 用
    micro_op_resolver.AddReshape();        // 输入 reshape 用
    micro_op_resolver.AddMean();           // Dense 层
    micro_op_resolver.AddFullyConnected(); // Dense 层
    micro_op_resolver.AddSoftmax();        // 输出 softmax

    // Build an interpreter to run the model with.
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        MicroPrintf("AllocateTensors() failed");
        return false;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    ESP_LOGI(TAG, "张量竞技场使用：%zu / %d 字节",
             interpreter->arena_used_bytes(), kTensorArenaSize);

    return true;
}

float *InferenceEngine::preprocess(float *raw_data, int len)
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

void InferenceEngine::run_normalized_inference(float *collected_data, int collected_data_index, int kNumTimeSteps, int kNumFeaturesPerStep)
{
    // 构建当前收集的数据为二维向量格式
    std::vector<std::vector<float>> raw_data(collected_data_index);
    for (int i = 0; i < collected_data_index; i++)
    {
        raw_data[i].resize(kNumFeaturesPerStep);
        for (int j = 0; j < kNumFeaturesPerStep; j++)
        {
            raw_data[i][j] = collected_data[i * kNumFeaturesPerStep + j];
        }
    }

    // 使用时间序列归一化函数将数据标准化为目标长度
    std::vector<std::vector<float>> normalized_data =
        normalize_mpu6050_data(raw_data, kNumTimeSteps);

    // 将归一化后的数据复制回模型输入张量
    for (int i = 0; i < kNumTimeSteps; i++)
    {
        for (int j = 0; j < kNumFeaturesPerStep; j++)
        {
            collected_data[i * kNumFeaturesPerStep + j] = normalized_data[i][j];
        }
    }
    float *input_data = preprocess(collected_data, kNumTimeSteps);

    // 现在使用归一化后的数据执行推理
    for (int i = 0; i < kNumTimeSteps * kNumFeaturesPerStep; i++)
    {
        input->data.f[i] = input_data[i];
    }

    // 执行推理
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "推理失败");
        return;
    }

    // 获取所有类别的概率
    num_classes_ = output->dims->data[1];
    float scores[32]; // 假设最多有32个类别，根据实际情况调整
    float max_score = -1.0f;
    predicted_class_ = 0;

    ESP_LOGI(TAG, "输出类别数：%d", num_classes_);
    for (int i = 0; i < num_classes_ && i < 32; i++)
    {
        scores[i] = output->data.f[i];
        all_scores_[i] = scores[i];
        ESP_LOGI(TAG, "score[%d] = %.4f", i, scores[i]);
        if (scores[i] > max_score)
        {
            max_score = scores[i];
            predicted_class_ = i;
        }
    }

    confidence_ = max_score;
    ESP_LOGI(TAG, "预测结果：类别 %d (置信度：%.4f)", predicted_class_, confidence_);
}