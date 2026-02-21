# 模型训练与测试指南

## 1. 环境准备

### 安装依赖
```bash
pip install -r requirements.txt
```

## 2. 模型训练

### 基础训练
```bash
python training.py
```
此脚本会：
- 从 dataset 目录读取训练数据
- 训练CNN模型
- 保存模型为 model.h5
- 保存预处理器为 scaler_mean.npy 和 scaler_scale.npy
- 保存标签映射为 label_map.txt

### 增强训练（推荐）
```bash
python enhanced_training.py
```
此脚本在基础训练基础上增加了：
- "未知"类别样本
- Dropout层防过拟合

## 3. 模型测试

### 基础测试
```bash
python test_model.py
```
使用置信度阈值过滤低质量预测

### 高级测试（推荐）
```bash
python advanced_test.py
```
使用置信度和预测熵双重判断未知样本

## 4. 处理高置信度误分类问题

对于像 `none_001.csv` 这样的文件，虽然被错误分类但置信度很高，可以采用以下方法：

### 方法1：置信度阈值
在 `test_model.py` 中设置 `CONFIDENCE_THRESHOLD` 参数，
当预测置信度低于该值时，将样本标记为 "unknown"

### 方法2：预测熵分析
在 `advanced_test.py` 中同时考虑：
- 预测置信度：最高概率值
- 预测熵：衡量预测分布的均匀性
当预测熵较高时，表示模型对各个类别的预测比较平均，不太确定

### 方法3：引入未知类别
在训练阶段添加"未知"类别样本，让模型学会区分已知和未知模式

## 5. 调整建议

针对 `none_001.csv` 被错误分类为 `triangle` 的问题：

1. 在 `advanced_test.py` 中调整阈值：
   - 降低 `CONFIDENCE_THRESHOLD` (默认0.7)
   - 降低 `ENTROPY_THRESHOLD` (默认0.4)

2. 收集更多"无手势"样本加入训练集

3. 使用增强训练脚本 `enhanced_training.py`