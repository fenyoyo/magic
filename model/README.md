# 陀螺仪运动轨迹识别TinyML系统

这是一个基于TensorFlow Lite的TinyML项目，用于识别陀螺仪传感器的运动轨迹模式，如三角形、圆形、正方形等几何形状。

## 项目概述

本项目旨在开发一个轻量级的机器学习模型，能够实时识别陀螺仪传感器采集的运动轨迹，并将其分类为不同的几何形状。该系统特别适用于嵌入式设备和物联网应用。

### 主要特性

- **实时轨迹识别**: 能够实时分析陀螺仪数据并识别运动模式
- **多类别分类**: 支持三角形、圆形、正方形和随机运动的分类
- **TinyML优化**: 模型经过优化，适合在资源受限的设备上运行
- **端到端解决方案**: 从数据生成到模型部署的完整流程

## 目录结构

```
paho/
├── generate_shapes_data.py      # 生成模拟陀螺仪数据
├── tinyml_gyro_classifier.py  # 原始模型训练脚本
├── tinyml_gyro_classifier_improved.py  # 改进模型训练脚本
├── tinyml_gyro_classifier_enhanced.py  # 增强模型训练脚本（100%准确率）
├── gyro_tinyml_system.py      # 系统验证脚本
├── infer_gyro_classifier.py   # 推理脚本
├── evaluate_enhanced_model.py # 评估增强模型的性能
├── visualize_gyro_data.py     # 数据可视化脚本
├── demo.py                    # 演示脚本
├── main.py                    # MQTT数据收集脚本
├── data/                      # 训练数据目录
├── triangle/                  # 三角形数据目录
├── gyro_shape_classifier_enhanced.h5    # 训练好的Keras模型（100%准确率）
├── gyro_shape_classifier_enhanced.tflite # TensorFlow Lite模型
├── scaler_enhanced.pkl        # 数据标准化器
├── training_history_enhanced.png # 训练历史图表
└── README.md
```

## 环境要求

- Python 3.8+
- TensorFlow 2.10+
- NumPy
- Pandas
- Matplotlib
- Scikit-learn

## 安装依赖

```bash
pip install numpy pandas matplotlib tensorflow scikit-learn joblib
```

## 快速开始

### 1. 数据生成

首先生成训练数据：

```bash
python generate_shapes_data.py
```

这将生成包含三角形、圆形、正方形和随机运动轨迹的模拟数据。

### 2. 模型训练

训练增强版TinyML模型（已达到100%准确率）：

```bash
python tinyml_gyro_classifier_enhanced.py
```

### 3. 模型验证

验证增强模型的性能：

```bash
python evaluate_enhanced_model.py
```

### 4. 演示

查看系统演示：

```bash
python demo.py
```

## 数据格式

模型接受包含以下列的CSV文件作为输入：
- `gx`, `gy`, `gz`: 陀螺仪角速度 (弧度/秒)
- `ax`, `ay`, `az`: 陀螺仪角加速度 (弧度/秒²)

## 模型架构

增强版模型使用1D卷积神经网络(CNN)和LSTM混合架构：
- 输入层: (250, 6) - 250个时间步长，6个特征
- 4个CNN块，逐步提取时空特征
- 2个LSTM层，捕获长期依赖关系
- 全连接层
- 输出层: 4个类别 (三角形、圆形、正方形、随机)

## TinyML部署

生成的TFLite模型(`gyro_shape_classifier_enhanced.tflite`)适合在以下设备上部署：
- Arduino Nano 33 BLE Sense
- ESP32系列
- Raspberry Pi Zero
- 其他支持TensorFlow Lite Micro的微控制器

## 训练数据

系统生成以下类别的训练数据：
- **Triangle (三角形)**: 200个样本（数据增强后）
- **Circle (圆形)**: 200个样本（数据增强后）
- **Square (正方形)**: 200个样本（数据增强后）
- **Random (随机)**: 200个样本（数据增强后）

每个样本包含250个时间步长的陀螺仪数据。

## 性能指标

- **模型大小**: ~550KB (Keras), ~450KB (TFLite)
- **参数数量**: ~114,000
- **支持的类别**: 4种几何形状
- **输入序列长度**: 250个时间步长
- **准确率**: 100%

## 使用场景

- 手势识别
- 运动分析
- 人机交互
- IoT设备控制
- 偯身追踪

## 项目改进历程

1. **初始模型**: 基础CNN架构，准确率较低
2. **改进模型**: 增加层数和正则化，准确率提升
3. **增强模型**: CNN+LSTM混合架构，数据增强，100%准确率

## 许可证

此项目仅供学习和研究使用。