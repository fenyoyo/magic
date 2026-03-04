import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow import keras
import os

# 配置参数
TIME_STEPS = 100
MODEL_PATH = './model.h5'
SCALER_MEAN_PATH = './scaler_mean.npy'
SCALER_SCALE_PATH = './scaler_scale.npy'
LABEL_MAP_PATH = './label_map.txt'

# 置信度阈值 - 低于此值认为是未知动作
DEFAULT_CONFIDENCE_THRESHOLD = 0.5

# 动作有效性检测阈值
# 加速度计的标准差阈值 - 检测是否有足够的动作变化
ACCEL_VARIANCE_THRESHOLD = 0.1
# 陀螺仪的标准差阈值
GYRO_VARIANCE_THRESHOLD = 0.1


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


def check_action_validity(data):
    """
    检查动作是否有效 - 检测是否有足够的动作变化
    
    返回：
        is_valid: 是否是有效动作
        metrics: 包含各项指标的字典
    """
    # 提取加速度计和陀螺仪数据
    accel = data[:, :3]  # ax, ay, az
    gyro = data[:, 3:]   # gx, gy, gz
    
    # 计算标准差
    accel_std = np.std(accel, axis=0)
    gyro_std = np.std(gyro, axis=0)
    
    # 计算总体变化幅度
    accel_variance = np.mean(accel_std)
    gyro_variance = np.mean(gyro_std)
    
    # 计算加速度幅值的变化
    accel_magnitude = np.sqrt(np.sum(accel ** 2, axis=1))
    accel_magnitude_std = np.std(accel_magnitude)
    
    # 判断是否有效
    is_valid = (accel_variance > ACCEL_VARIANCE_THRESHOLD or 
                gyro_variance > GYRO_VARIANCE_THRESHOLD)
    
    # 检测是否是静止状态（加速度计主要受重力影响，应该接近 1g）
    is_stationary = (accel_magnitude_std < 0.05 and 
                     np.all(accel_std < 0.1))
    
    if is_stationary:
        is_valid = False
    
    metrics = {
        'accel_variance': accel_variance,
        'gyro_variance': gyro_variance,
        'accel_magnitude_std': accel_magnitude_std,
        'is_stationary': is_stationary
    }
    
    return is_valid, metrics


def calculate_entropy(probabilities):
    """
    计算概率分布的熵值
    熵值越高，表示模型越不确定
    """
    # 避免 log(0)
    probs = np.clip(probabilities, 1e-10, 1.0)
    entropy = -np.sum(probabilities * np.log(probs))
    # 归一化到 0-1 范围（除以最大可能熵值）
    max_entropy = np.log(len(probabilities))
    normalized_entropy = entropy / max_entropy if max_entropy > 0 else 0
    return normalized_entropy


def predict_from_csv(csv_path, model, scaler_mean, scaler_scale, idx_to_label, 
                     top_k=3, confidence_threshold=DEFAULT_CONFIDENCE_THRESHOLD,
                     check_validity=True):
    """
    从 CSV 文件进行预测
    
    参数:
        confidence_threshold: 置信度阈值，低于此值认为是未知动作
        check_validity: 是否检查动作有效性
    
    返回:
        dict: 包含预测结果和状态
            - is_valid_action: 是否是有效动作
            - is_unknown: 是否是未知动作
            - prediction: 预测结果列表（如果是未知动作则为空）
            - confidence: 最高置信度
            - entropy: 熵值（不确定性指标）
            - validity_metrics: 动作有效性指标（如果 check_validity=True）
    """
    # 读取数据
    df = pd.read_csv(csv_path)
    data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values
    
    result = predict_from_array(data, model, scaler_mean, scaler_scale, idx_to_label,
                                top_k, confidence_threshold, check_validity)
    return result


def predict_from_array(data, model, scaler_mean, scaler_scale, idx_to_label,
                       top_k=3, confidence_threshold=DEFAULT_CONFIDENCE_THRESHOLD,
                       check_validity=True):
    """
    从 numpy 数组进行预测
    
    参数:
        data: (N, 6) 格式的数组，包含 ax, ay, az, gx, gy, gz
        confidence_threshold: 置信度阈值
        check_validity: 是否检查动作有效性
    
    返回:
        dict: 包含预测结果和状态
    """
    result = {
        'is_valid_action': True,
        'is_unknown': False,
        'prediction': [],
        'confidence': 0.0,
        'entropy': 0.0,
        'validity_metrics': None
    }
    
    # 检查动作有效性
    if check_validity:
        is_valid, metrics = check_action_validity(data)
        result['validity_metrics'] = metrics
        result['is_valid_action'] = is_valid
        
        if not is_valid:
            result['is_unknown'] = True
            return result
    
    # 预处理
    processed_data = preprocess_data(data, scaler_mean, scaler_scale)
    
    # 预测
    predictions = model.predict(processed_data, verbose=0)
    probs = predictions[0]
    
    # 计算熵值
    entropy = calculate_entropy(probs)
    result['entropy'] = entropy
    
    # 获取最高概率
    top_idx = np.argmax(probs)
    top_prob = probs[top_idx]
    result['confidence'] = float(top_prob)
    
    # 检查置信度
    if top_prob < confidence_threshold:
        result['is_unknown'] = True
        # 即使置信度低，也返回预测结果供参考
        top_indices = np.argsort(probs)[::-1][:top_k]
        for idx in top_indices:
            result['prediction'].append({
                'label': idx_to_label[idx],
                'probability': float(probs[idx])
            })
        return result
    
    # 获取概率最高的前 K 个结果
    top_indices = np.argsort(probs)[::-1][:top_k]
    for idx in top_indices:
        result['prediction'].append({
            'label': idx_to_label[idx],
            'probability': float(probs[idx])
        })
    
    return result


def print_prediction_result(result):
    """打印预测结果"""
    print("\n" + "="*50)
    
    if not result['is_valid_action']:
        print("❌ 无效动作 - 未检测到明显的手势动作")
        if result['validity_metrics']:
            metrics = result['validity_metrics']
            print(f"   加速度变化：{metrics['accel_variance']:.4f}")
            print(f"   陀螺仪变化：{metrics['gyro_variance']:.4f}")
            print(f"   静止状态：{metrics['is_stationary']}")
        return
    
    if result['is_unknown']:
        print("❓ 未知动作 - 无法识别为已知手势")
        print(f"   最高置信度：{result['confidence']:.2%}")
        print(f"   不确定性 (熵): {result['entropy']:.4f}")
        
        if result['prediction']:
            print("\n   参考预测（置信度低于阈值）:")
            for i, pred in enumerate(result['prediction'], 1):
                print(f"     {i}. {pred['label']}: {pred['probability']:.2%}")
    else:
        print("✅ 识别成功")
        print(f"   预测结果：{result['prediction'][0]['label']}")
        print(f"   置信度：{result['confidence']:.2%}")
        print(f"   不确定性 (熵): {result['entropy']:.4f}")
        
        if len(result['prediction']) > 1:
            print("\n   其他可能:")
            for i, pred in enumerate(result['prediction'][1:], 2):
                print(f"     {i}. {pred['label']}: {pred['probability']:.2%}")


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
        result = predict_from_csv(sample_csv, model, scaler_mean, scaler_scale, 
                                  idx_to_label, confidence_threshold=0.6)
        print_prediction_result(result)


if __name__ == '__main__':
    sample_csv = 'data/circle_001.csv'
    premain(sample_csv)