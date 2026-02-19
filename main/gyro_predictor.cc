/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "gyro_predictor.h"
#include "esp_log.h"

#define TAG "GyroPredictor"

GyroPredictor::GyroPredictor()
    : model_(nullptr), interpreter_(nullptr), input_(nullptr), output_(nullptr)
{
}

GyroPredictor::~GyroPredictor()
{
    // 清理资源
    // 注意：由于使用的是静态解释器，在栈上分配，所以不需要显式删除
}

bool GyroPredictor::Init()
{
    // 获取模型
    model_ = tflite::GetModel(gyro_model);
    if (model_->version() != TFLITE_SCHEMA_VERSION)
    {
        // ESP_LOGE(TAG, "Model provided is schema version %d not equal to supported version %d.",
        //          model_->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    // 创建操作解析器并注册所需的操作
    static tflite::MicroMutableOpResolver<5> resolver; // 支持最多5种操作

    TfLiteStatus resolver_status;
    resolver_status = resolver.AddFullyConnected();
    if (resolver_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AddFullyConnected failed");
        return false;
    }

    resolver_status = resolver.AddConv2D();
    if (resolver_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AddConv2D failed");
        return false;
    }

    resolver_status = resolver.AddDepthwiseConv2D();
    if (resolver_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AddDepthwiseConv2D failed");
        return false;
    }

    resolver_status = resolver.AddSoftmax();
    if (resolver_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AddSoftmax failed");
        return false;
    }

    resolver_status = resolver.AddReshape();
    if (resolver_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AddReshape failed");
        return false;
    }

    // 创建解释器
    static tflite::MicroInterpreter static_interpreter(
        model_, resolver, tensor_arena_, kTensorArenaSize);
    interpreter_ = &static_interpreter;

    // 分配张量内存
    TfLiteStatus allocate_status = interpreter_->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AllocateTensors() failed");
        return false;
    }

    // 获取输入和输出张量
    input_ = interpreter_->input(0);
    output_ = interpreter_->output(0);

    ESP_LOGI(TAG, "Model initialized successfully");
    ESP_LOGI(TAG, "Input tensor shape: [%d]", input_->dims->size);
    for (int i = 0; i < input_->dims->size; i++)
    {
        ESP_LOGI(TAG, "  Input dim[%d]: %d", i, input_->dims->data[i]);
    }
    ESP_LOGI(TAG, "Output tensor shape: [%d]", output_->dims->size);
    for (int i = 0; i < output_->dims->size; i++)
    {
        ESP_LOGI(TAG, "  Output dim[%d]: %d", i, output_->dims->data[i]);
    }

    return true;
}

bool GyroPredictor::Predict(float *input_data, int input_size, float *output_data, int output_size)
{
    if (!interpreter_ || !input_ || !output_)
    {
        ESP_LOGE(TAG, "Predictor not initialized properly");
        return false;
    }

    // 检查输入维度是否匹配
    if (input_size != input_->bytes / sizeof(float))
    {
        ESP_LOGE(TAG, "Input size mismatch: expected %d, got %d",
                 input_->bytes / sizeof(float), input_size);
        return false;
    }

    // 将输入数据量化并复制到模型输入张量
    if (input_->type == kTfLiteFloat32)
    {
        // 浮点模型 - 直接复制
        float *input_buffer = input_->data.f;
        for (int i = 0; i < input_size; i++)
        {
            input_buffer[i] = input_data[i];
        }
    }
    else if (input_->type == kTfLiteInt8)
    {
        // 整数模型 - 需要量化
        int8_t *input_buffer = input_->data.int8;
        for (int i = 0; i < input_size; i++)
        {
            input_buffer[i] = static_cast<int8_t>(input_data[i] / input_->params.scale + input_->params.zero_point);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported input tensor type: %d", input_->type);
        return false;
    }

    // 执行推理
    TfLiteStatus invoke_status = interpreter_->Invoke();
    if (invoke_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "Invoke failed");
        return false;
    }

    // 从输出张量复制结果并反量化
    int output_tensor_size = output_->bytes / (output_->type == kTfLiteInt8 ? sizeof(int8_t) : sizeof(float));
    int copy_size = (output_size < output_tensor_size) ? output_size : output_tensor_size;

    if (output_->type == kTfLiteFloat32)
    {
        // 浮点输出 - 直接复制
        float *output_buffer = output_->data.f;
        for (int i = 0; i < copy_size; i++)
        {
            output_data[i] = output_buffer[i];
        }
    }
    else if (output_->type == kTfLiteInt8)
    {
        // 整数输出 - 需要反量化
        int8_t *output_buffer = output_->data.int8;
        for (int i = 0; i < copy_size; i++)
        {
            output_data[i] = (output_buffer[i] - output_->params.zero_point) * output_->params.scale;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported output tensor type: %d", output_->type);
        return false;
    }

    return true;
}