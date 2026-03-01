import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import load_model
import joblib
from time_normalization import time_normalize_sequence
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.metrics import accuracy_score, precision_recall_fscore_support


class GesturePredictor:
    def __init__(self, model_path="gesture_recognition_model.keras"):
        """初始化手势预测器"""
        try:
            self.model = load_model(model_path)
        except:
            # 如果.keras格式不可用，尝试加载.h5格式
            self.model = load_model(model_path.replace('.keras', '.h5'))
        
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
            raise ValueError("CSV file must contain ax, ay, az, gx, gy, gz columns")
        
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
        bars = plt.bar(self.label_names, all_predictions)
        plt.title(f'Prediction Probabilities for {predicted_label}')
        plt.xlabel('Gesture Class')
        plt.ylabel('Probability')
        plt.xticks(rotation=45)
        
        # 在柱状图上显示数值
        for bar, prob in zip(bars, all_predictions):
            plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.01, 
                    f'{prob:.2f}', ha='center', va='bottom')
        
        plt.grid(True, linestyle='--', alpha=0.3)
        plt.tight_layout()
        plt.show()
        
        return predicted_label, confidence

    def batch_predict(self, csv_paths):
        """批量预测多个CSV文件"""
        results = []
        for csv_path in csv_paths:
            try:
                predicted_label, confidence, _ = self.predict_from_csv(csv_path)
                results.append({
                    'file': csv_path,
                    'prediction': predicted_label,
                    'confidence': confidence
                })
            except Exception as e:
                results.append({
                    'file': csv_path,
                    'prediction': 'ERROR',
                    'confidence': 0.0,
                    'error': str(e)
                })
        return results

    def evaluate_on_dataset(self, dataset_path="dataset"):
        """在数据集上评估模型性能"""
        print("Evaluating model on dataset...")
        
        true_labels = []
        pred_labels = []
        
        for gesture_class in os.listdir(dataset_path):
            gesture_path = os.path.join(dataset_path, gesture_class)
            if not os.path.isdir(gesture_path):
                continue
            
            print(f"Evaluating class: {gesture_class}")
            
            for csv_file in os.listdir(gesture_path):
                if csv_file.endswith('.csv'):
                    csv_path = os.path.join(gesture_path, csv_file)
                    try:
                        predicted_label, confidence, _ = self.predict_from_csv(csv_path)
                        
                        true_labels.append(gesture_class)
                        pred_labels.append(predicted_label)
                    except Exception as e:
                        print(f"Error processing {csv_file}: {str(e)}")
        
        # 计算准确率
        accuracy = accuracy_score(true_labels, pred_labels)
        precision, recall, f1, _ = precision_recall_fscore_support(true_labels, pred_labels, average='weighted')
        
        print(f"\nDataset Evaluation Results:")
        print(f"Accuracy: {accuracy:.4f}")
        print(f"Precision: {precision:.4f}")
        print(f"Recall: {recall:.4f}")
        print(f"F1-Score: {f1:.4f}")
        
        # 显示详细分类报告
        from sklearn.metrics import classification_report
        print("\nDetailed Classification Report:")
        print(classification_report(true_labels, pred_labels))
        
        return {
            'accuracy': accuracy,
            'precision': precision,
            'recall': recall,
            'f1_score': f1
        }


def main():
    # 创建预测器实例
    predictor = GesturePredictor()
    
    # 评估模型在数据集上的表现
    print("Evaluating model on dataset...")
    evaluation_results = predictor.evaluate_on_dataset()
    
    # 示例：使用测试数据进行预测
    import os
    test_files = []
    for root, dirs, files in os.walk("test"):
        for file in files:
            if file.endswith(".csv"):
                test_files.append(os.path.join(root, file))
    
    if test_files:
        print(f"\nUsing {len(test_files)} test files for prediction:")
        for test_file in test_files[:3]:  # 只测试前3个文件
            try:
                predicted_label, confidence = predictor.visualize_prediction(test_file)
                print(f"File: {test_file}")
                print(f"Predicted gesture: {predicted_label}, Confidence: {confidence:.4f}")
                print("-" * 50)
            except Exception as e:
                print(f"Error predicting {test_file}: {str(e)}")
    else:
        print("No test files found, demonstrating with simulated data")
        
        # 使用模拟数据演示预测功能
        print("\nDemonstrating with simulated data:")
        # 生成模拟数据 (实际应用中，这些数据来自传感器)
        sample_data = np.random.random((100, 6)).astype(np.float32)  # 100个时间步，6个特征
        predicted_label, confidence, _ = predictor.predict_from_raw_data(sample_data)
        print(f"Predicted gesture: {predicted_label}, Confidence: {confidence:.4f}")


if __name__ == "__main__":
    main()