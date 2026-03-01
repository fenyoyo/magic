import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import load_model
import joblib
from time_normalization import time_normalize_sequence
import matplotlib.pyplot as plt


class GesturePredictor:
    def __init__(self, model_path="gesture_recognition_model.h5"):
        """初始化手势预测器"""
        self.model = load_model(model_path)
        self.scaler = joblib.load("scaler.pkl")
        self.label_encoder = joblib.load("label_encoder.pkl")
        
        # 获取标签名称
        self.label_names = self.label_encoder.classes_
        
    def predict_from_csv(self, csv_path, sequence_length=100):
        """从CSV文件预测手势"""
        # 读取CSV文件
        df = pd.read_csv(csv_path)
        
        # 提取陀螺仪和加速度计数据
        if all(col in df.columns for col in ['ax', 'ay', 'az', 'gx', 'gy', 'gz']):
            data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values.astype(np.float32)
        else:
            raise ValueError("CSV文件必须包含 ax, ay, az, gx, gy, gz 列")
        
        # 时间归一化
        data = time_normalize_sequence(data, sequence_length)
        
        # 添加批次维度
        data = np.expand_dims(data, axis=0)
        
        # 标准化
        original_shape = data.shape
        data = data.reshape(-1, data.shape[-1])
        data = self.scaler.transform(data)
        data = data.reshape(original_shape)
        
        # 预测
        predictions = self.model.predict(data)
        predicted_class_idx = np.argmax(predictions[0])
        confidence = predictions[0][predicted_class_idx]
        
        predicted_label = self.label_names[predicted_class_idx]
        
        return predicted_label, confidence, predictions[0]
    
    def predict_from_raw_data(self, raw_data, sequence_length=100):
        """从原始数据数组预测手势
        raw_data: shape (sequence_length, 6) 的数组，包含 [ax, ay, az, gx, gy, gz] 数据
        """
        # 时间归一化
        data = time_normalize_sequence(raw_data, sequence_length)
        
        # 添加批次维度
        data = np.expand_dims(data, axis=0)
        
        # 标准化
        original_shape = data.shape
        data = data.reshape(-1, data.shape[-1])
        data = self.scaler.transform(data)
        data = data.reshape(original_shape)
        
        # 预测
        predictions = self.model.predict(data)
        predicted_class_idx = np.argmax(predictions[0])
        confidence = predictions[0][predicted_class_idx]
        
        predicted_label = self.label_names[predicted_class_idx]
        
        return predicted_label, confidence, predictions[0]
    
    def visualize_prediction(self, csv_path, sequence_length=100):
        """可视化预测结果"""
        predicted_label, confidence, all_predictions = self.predict_from_csv(csv_path, sequence_length)
        
        # 读取数据用于可视化
        df = pd.read_csv(csv_path)
        timesteps = range(len(df))
        
        fig, axes = plt.subplots(2, 1, figsize=(14, 10))
        
        # 绘制加速度计数据
        axes[0].plot(timesteps, df['ax'], label='ax (X-axis acceleration)', color='red', alpha=0.7)
        axes[0].plot(timesteps, df['ay'], label='ay (Y-axis acceleration)', color='green', alpha=0.7)
        axes[0].plot(timesteps, df['az'], label='az (Z-axis acceleration)', color='blue', alpha=0.7)
        axes[0].set_title(f'Accelerometer Data - Predicted: {predicted_label} (Confidence: {confidence:.2f})')
        axes[0].set_xlabel('Time Step')
        axes[0].set_ylabel('Acceleration Value')
        axes[0].legend()
        axes[0].grid(True, linestyle='--', alpha=0.3)
        
        # 绘制陀螺仪数据
        axes[1].plot(timesteps, df['gx'], label='gx (X-axis rotation)', color='orange', alpha=0.7)
        axes[1].plot(timesteps, df['gy'], label='gy (Y-axis rotation)', color='purple', alpha=0.7)
        axes[1].plot(timesteps, df['gz'], label='gz (Z-axis rotation)', color='brown', alpha=0.7)
        axes[1].set_title('Gyroscope Data')
        axes[1].set_xlabel('Time Step')
        axes[1].set_ylabel('Angular Velocity Value')
        axes[1].legend()
        axes[1].grid(True, linestyle='--', alpha=0.3)
        
        plt.tight_layout()
        plt.show()
        
        # 显示所有类别的预测概率
        plt.figure(figsize=(10, 6))
        plt.bar(self.label_names, all_predictions)
        plt.title(f'Prediction Probabilities for {predicted_label}')
        plt.xlabel('Gesture Class')
        plt.ylabel('Probability')
        plt.xticks(rotation=45)
        plt.grid(True, linestyle='--', alpha=0.3)
        plt.tight_layout()
        plt.show()
        
        return predicted_label, confidence


def main():
    # 创建预测器实例
    predictor = GesturePredictor()
    
    # 示例：使用测试数据进行预测
    test_files = []
    for root, dirs, files in os.walk("test"):
        for file in files:
            if file.endswith(".csv"):
                test_files.append(os.path.join(root, file))
    
    if test_files:
        print("使用测试文件进行预测:")
        for test_file in test_files[:3]:  # 只测试前3个文件
            try:
                predicted_label, confidence, _ = predictor.predict_from_csv(test_file)
                print(f"文件: {test_file}")
                print(f"预测手势: {predicted_label}, 置信度: {confidence:.4f}")
                print("-" * 50)
                
                # 可视化第一个预测
                if test_files.index(test_file) == 0:
                    predictor.visualize_prediction(test_file)
            except Exception as e:
                print(f"预测文件 {test_file} 时出错: {str(e)}")
    else:
        print("没有找到测试文件，您可以提供一个CSV文件路径进行预测")
        # 示例如何使用原始数据进行预测
        print("\n示例 - 使用随机生成的数据进行预测:")
        # 生成模拟数据 (实际应用中，这些数据来自传感器)
        sample_data = np.random.random((100, 6)).astype(np.float32)  # 100个时间步，6个特征
        predicted_label, confidence, _ = predictor.predict_from_raw_data(sample_data)
        print(f"预测手势: {predicted_label}, 置信度: {confidence:.4f}")


if __name__ == "__main__":
    import os
    main()