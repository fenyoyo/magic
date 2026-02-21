import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.preprocessing import StandardScaler
from tensorflow import keras

def load_model_and_preprocessor(model_path='./output/model.h5', scaler_mean_path='./output/scaler_mean.npy',
                               scaler_scale_path='./output/scaler_scale.npy', label_map_path='./output/label_map.txt'):
    """
    加载训练好的模型和预处理器
    """
    # 加载模型
    model = tf.keras.models.load_model(model_path)
    
    # 加载标准化器参数
    mean = np.load(scaler_mean_path)
    scale = np.load(scaler_scale_path)
    
    scaler = StandardScaler()
    scaler.mean_ = mean
    scaler.scale_ = scale
    
    # 加载标签映射
    label_map = {}
    with open(label_map_path, 'r') as f:
        for line in f:
            label, idx = line.strip().split(':')
            label_map[int(idx)] = label
    
    return model, scaler, label_map

def preprocess_single_sample(file_path, scaler, time_steps=100):
    """
    预处理单个样本
    """
    df = pd.read_csv(file_path)
    data = df[['ax','ay','az','gx','gy','gz']].values

    if len(data) >= time_steps:
        data = data[:time_steps]
    else:
        pad = np.zeros((time_steps-len(data), 6))
        data = np.vstack((data, pad))

    # 应用标准化
    data_reshaped = data.reshape(-1, 6)
    data_scaled = scaler.transform(data_reshaped)
    data_final = data_scaled.reshape(1, time_steps, 6)
    
    return data_final

def test_single_file(model, scaler, label_map, file_path):
    """
    测试单个文件
    """
    processed_data = preprocess_single_sample(file_path, scaler)
    prediction = model.predict(processed_data)
    predicted_class_idx = np.argmax(prediction, axis=1)[0]
    confidence = np.max(prediction)
    
    predicted_label = label_map[predicted_class_idx]
    
    filename = os.path.basename(file_path)
    print(f"文件: {filename}")
    print(f"预测类别: {predicted_label}")
    print(f"置信度: {confidence:.4f}")
    print(f"预测概率分布: {prediction[0]}")
    print("-" * 50)
    
    return predicted_label, confidence

def test_all_files_in_directory(model, scaler, label_map, test_dir='test'):
    """
    测试目录下所有CSV文件
    """
    test_files = [f for f in os.listdir(test_dir) if f.lower().endswith('.csv')]
    
    if not test_files:
        print(f"在 {test_dir} 目录中没有找到CSV文件")
        return
    
    print(f"开始测试 {len(test_files)} 个文件...")
    print("=" * 50)
    
    for file in test_files:
        file_path = os.path.join(test_dir, file)
        test_single_file(model, scaler, label_map, file_path)

if __name__ == "__main__":
    # 加载模型和预处理器
    print("正在加载模型和预处理器...")
    model, scaler, label_map = load_model_and_preprocessor()
    
    print(f"模型加载成功！")
    print(f"标签映射: {label_map}")
    print("=" * 50)
    
    # 测试 test 目录下的所有文件
    test_all_files_in_directory(model, scaler, label_map, 'test')