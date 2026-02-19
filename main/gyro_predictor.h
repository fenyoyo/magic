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

#ifndef GYRO_PREDICTOR_H_
#define GYRO_PREDICTOR_H_

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "gyro_model_data.h"  // 包含你的陀螺仪模型数据

class GyroPredictor {
public:
    // 构造函数
    GyroPredictor();
    
    // 析构函数
    ~GyroPredictor();
    
    // 初始化预测器
    bool Init();
    
    // 执行预测
    bool Predict(float* input_data, int input_size, float* output_data, int output_size);
    
private:
    const tflite::Model* model_;
    tflite::MicroInterpreter* interpreter_;
    TfLiteTensor* input_;
    TfLiteTensor* output_;
    
    // 为模型分配内存的缓冲区
    static constexpr int kTensorArenaSize = 8192;  // 增加缓冲区大小以适应更复杂的模型
    uint8_t tensor_arena_[kTensorArenaSize];
};

#endif  // GYRO_PREDICTOR_H_