# 数据可视化对比功能说明

## 功能概述

新增了一个功能，用于对比原始传感器数据和经过时间归一化处理后的数据。该功能可以帮助用户直观地理解时间归一化算法的效果。

## 新增函数

### compare_original_and_normalized(file_path, target_length=100)

该函数创建一个包含6个子图的对比图表：

1. **原始加速度计数据** - 显示原始长度的ax、ay、az数据
2. **归一化后加速度计数据** - 显示固定长度（默认100）的ax、ay、az数据
3. **原始陀螺仪数据** - 显示原始长度的gx、gy、gz数据
4. **归一化后陀螺仪数据** - 显示固定长度的gx、gy、gz数据
5. **原始ax数据** - 单独显示ax数据作为示例
6. **归一化后ax数据** - 显示归一化后的ax数据

## 使用方法

```python
from visualize_gyro_data import compare_original_and_normalized

# 对比数据
compare_original_and_normalized('path/to/your/data.csv', target_length=100)
```

## 输出信息

除了可视化图表，函数还会输出统计数据对比：
- 原始数据和归一化数据的平均值
- 原始数据和归一化数据的标准差

## 技术特点

- 使用线性插值算法进行时间归一化
- 保持数据的趋势和关键特征
- 支持任意长度的输入数据归一化到固定长度
- 提供详细的可视化对比，便于分析算法效果

## 文件说明

- `visualize_gyro_data.py` - 原有的可视化文件，已更新包含新功能
- `compare_visualization.py` - 英文版对比可视化脚本，避免中文字符显示问题