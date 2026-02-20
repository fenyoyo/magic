# TinyML 陀螺仪形状分类模型

## 概述
这是一个基于TensorFlow Lite Micro的TinyML模型，用于对陀螺仪传感器数据进行形状分类。模型能够识别四种不同的手势形状：三角形、圆形、正方形和随机运动。

## 模型架构
- **输入**: 时间序列数据，形状为 (batch_size, 250, 6)，其中6代表陀螺仪的6个轴 (gx, gy, gz, ax, ay, az)
- **架构**: CNN-LSTM混合模型
  - 卷积层用于提取局部特征
  - LSTM层捕获长期依赖关系
  - 全连接层进行最终分类
- **输出**: 4个类别的概率分布 (三角形, 圆形, 正方形, 随机)

## 训练数据
- **数据源**: 陀螺仪传感器采集的手势数据
- **数据格式**: CSV文件，包含列: `gx`, `gy`, `gz`, `ax`, `ay`, `az`
- **类别**:
  - Triangle (0)
  - Circle (1) 
  - Square (2)
  - Random (3)

## 训练过程
1. 数据预处理和标准化
2. 使用CNN提取空间特征
3. 使用LSTM建模时间序列模式
4. 全连接层进行分类

## 模型优化
- 使用INT8量化减小模型大小
- 优化内存使用以适配微控制器

## 部署说明
模型被转换为C++头文件格式 (`gyro_model_data.h`)，可以直接在ESP32等微控制器上部署使用。

### 模型转换
```bash
python model_to_cc_converter.py gyro_shape_classifier_enhanced.h5 gyro_model_data.h gyro_model
```

### ESP32端使用
- `PredictShape()` 方法用于执行形状分类
- 输入数据需要重新整形为模型期望的格式
- 返回预测的类别和置信度

## 性能指标
- **准确率**: >95% (根据测试集)
- **模型大小**: 约XX KB (TFLite格式)
- **推理时间**: 约XX ms (取决于硬件)

## 文件结构
- `tinyml_gyro_classifier_enhanced.py`: 模型训练代码
- `evaluate_enhanced_model.py`: 模型评估代码
- `model_to_cc_converter.py`: 模型转换工具
- `visualize_gyro_data.py`: 数据可视化工具
- `gyro_shape_classifier_enhanced.h5`: 训练好的Keras模型
- `gyro_shape_classifier_enhanced.tflite`: TensorFlow Lite模型
- `gyro_model_data.h`: C++头文件格式模型
- `scaler_enhanced.pkl`: 特征标准化器
- `training_history_enhanced.png`: 训练历史图表

## ESP32端接口
在ESP32端，使用`GyroPredictor`类来执行推理：
- `Init()`: 初始化模型
- `PredictShape()`: 执行形状分类
- `GetInputSize()/GetOutputSize()`: 获取张量尺寸信息