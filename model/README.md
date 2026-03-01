# 陀螺仪手势识别系统

这个项目使用深度学习技术基于陀螺仪和加速度计数据来识别手势，如字母A、字母C等。

## 项目结构

```
model/
├── dataset/                 # 训练数据集
│   ├── letter_a/           # 字母A的手势数据
│   ├── letter_c/           # 字母C的手势数据
│   ├── up/                 # 向上手势数据
│   ├── down/               # 向下手势数据
│   ├── left/               # 向左手势数据
│   └── right/              # 向右手势数据
├── gesture_recognition_training.py  # 训练脚本
├── gesture_predictor.py             # 预测脚本
├── time_normalization.py            # 时间归一化工具
└── README.md
```

## 数据格式

训练数据应该是CSV格式，包含以下列：
- `ax`, `ay`, `az`: 三轴加速度计数据
- `gx`, `gy`, `gz`: 三轴陀螺仪数据

示例CSV文件结构：
```csv
ax,ay,az,gx,gy,gz
0.1,0.2,9.8,0.01,0.02,0.03
0.15,0.25,9.75,0.02,0.03,0.04
...
```

## 如何使用

### 1. 准备数据

将你的训练数据按照手势类别放在`dataset`目录下，每个手势类别一个子目录：

```
dataset/
├── letter_a/
│   ├── example_001.csv
│   ├── example_002.csv
│   └── ...
├── letter_c/
│   ├── example_001.csv
│   ├── example_002.csv
│   └── ...
└── ...
```

### 2. 训练模型

运行训练脚本：

```bash
python gesture_recognition_training.py
```

训练过程会：
- 自动加载所有数据
- 构建CNN-LSTM混合模型
- 训练模型并显示进度
- 保存最佳模型和预处理器

### 3. 使用模型进行预测

运行预测脚本：

```bash
python gesture_predictor.py
```

或者在代码中使用：

```python
from gesture_predictor import GesturePredictor

predictor = GesturePredictor()
predicted_label, confidence, probabilities = predictor.predict_from_csv("path/to/your/data.csv")
print(f"预测手势: {predicted_label}, 置信度: {confidence:.4f}")
```

## 模型架构

使用CNN-LSTM混合架构：
- CNN层提取局部特征
- LSTM层捕获时序关系
- 全连接层进行最终分类

## 添加新手势

要添加新的手势类型（如字母A）：

1. 在`dataset`目录下创建新文件夹（如`letter_a`）
2. 收集该手势的多个样本数据
3. 每个样本保存为单独的CSV文件
4. 重新运行训练脚本

## 注意事项

- 确保数据采样频率一致
- 数据预处理会自动进行时间归一化
- 模型会自动保存最佳权重
- 可以通过修改训练脚本调整模型参数