import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow import keras
import os

# 配置参数
TIME_STEPS = 100
MODEL_PATH = '/model.h5'
SCALER_MEAN_PATH = '/scaler_mean.npy'
SCALER_SCALE_PATH = '/scaler_scale.npy'
LABEL_MAP_PATH = '/label_map.txt'


def load_model_and_preprocessors():
    """加载模型和预处理器"""
    print("正在加载模型和预处理器...")
    
    # 加载模型
    model = keras.models.load_model(MODEL_PATH)
    
    # 加载标准化参数
    scaler_mean = np.load(SCALER_MEAN_PATH)
    scaler_scale = np.load(SCALER_SCALE_PATH)
    
    # 加载标签映射
    label_map = {}
    with open(LABEL_MAP_PATH, 'r', encoding='utf-8') as f:
        for line in f:
            label, idx = line.strip().split(':')
            label_map[label] = int(idx)
    
    # 创建反向映射
    idx_to_label = {idx: label for label, idx in label_map.items()}
    
    print(f"模型加载完成，共 {len(label_map)} 个类别")
    return model, scaler_mean, scaler_scale, label_map, idx_to_label


def time_normalize_sequence(data, target_length):
    """使用线性插值进行时间归一化"""
    from scipy.interpolate import interp1d
    
    original_length = len(data)
    
    if original_length == target_length:
        return data
    
    # 创建原始数据的时间点
    original_time = np.linspace(0, 1, original_length)
    # 创建目标时间点
    target_time = np.linspace(0, 1, target_length)
    
    # 对每个特征进行插值
    normalized_data = np.zeros((target_length, data.shape[1]))
    for i in range(data.shape[1]):
        f = interp1d(original_time, data[:, i], kind='linear')
        normalized_data[:, i] = f(target_time)
    
    return normalized_data


def preprocess_data(data, scaler_mean, scaler_scale):
    """预处理输入数据"""
    # 时间归一化
    data_normalized = time_normalize_sequence(data, TIME_STEPS)
    
    # 标准化
    data_reshaped = data_normalized.reshape(-1, 6)
    data_scaled = (data_reshaped - scaler_mean) / scaler_scale
    data_final = data_scaled.reshape(1, TIME_STEPS, 6)
    
    return data_final


def predict_from_csv(csv_path, model, scaler_mean, scaler_scale, idx_to_label, top_k=3):
    """从 CSV 文件进行预测"""
    # 读取数据
    df = pd.read_csv(csv_path)
    data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values
    
    # 预处理
    processed_data = preprocess_data(data, scaler_mean, scaler_scale)
    
    # 预测
    predictions = model.predict(processed_data, verbose=0)
    
    # 获取概率最高的前 K 个结果
    top_indices = np.argsort(predictions[0])[::-1][:top_k]
    
    results = []
    for idx in top_indices:
        label = idx_to_label[idx]
        probability = predictions[0][idx]
        results.append({
            'label': label,
            'probability': probability
        })
    
    return results


def predict_from_array(data, model, scaler_mean, scaler_scale, idx_to_label, top_k=3):
    """从 numpy 数组进行预测"""
    # data 应该是 (N, 6) 的数组，包含 ax, ay, az, gx, gy, gz
    
    # 预处理
    processed_data = preprocess_data(data, scaler_mean, scaler_scale)
    
    # 预测
    predictions = model.predict(processed_data, verbose=0)
    
    # 获取概率最高的前 K 个结果
    top_indices = np.argsort(predictions[0])[::-1][:top_k]
    
    results = []
    for idx in top_indices:
        label = idx_to_label[idx]
        probability = predictions[0][idx]
        results.append({
            'label': label,
            'probability': probability
        })
    
    return results


def premain(sample_csv):
    """主函数 - 示例用法"""
    # 加载模型
    model, scaler_mean, scaler_scale, label_map, idx_to_label = load_model_and_preprocessors()
    
    # 示例：从 CSV 文件预测
    print("\n" + "="*50)
    print("示例：从 CSV 文件预测")
    print("="*50)
    
    # 使用 data 目录下的示例文件

    if os.path.exists(sample_csv):
        results = predict_from_csv(sample_csv, model, scaler_mean, scaler_scale, idx_to_label)
        print(f"\n文件：{sample_csv}")
        print("预测结果:")
        for i, result in enumerate(results, 1):
            print(f"  {i}. {result['label']}: {result['probability']:.2%}")


if __name__ == '__main__':
    sample_csv = 'data/circle_001.csv'
    premain(sample_csv)