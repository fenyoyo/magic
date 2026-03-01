"""
陀螺仪手势识别系统 - 使用指南

此脚本演示了如何使用训练好的模型进行手势识别
"""

import os
import numpy as np
import pandas as pd
from enhanced_predictor import GesturePredictor
from final_training import GestureRecognitionTrainer


def demo_training():
    """演示模型训练过程"""
    print("="*60)
    print("GESTURE RECOGNITION MODEL TRAINING DEMO")
    print("="*60)
    
    # 创建训练器实例
    trainer = GestureRecognitionTrainer(dataset_path="dataset", target_length=100)
    
    # 训练模型并进行验证
    print("\nStarting training process...")
    model, history, test_accuracy = trainer.train_with_validation(epochs=50, batch_size=16)
    
    print(f"\nTraining completed with test accuracy: {test_accuracy:.4f}")
    
    # 绘制训练历史
    trainer.plot_training_history()
    
    # 显示标签映射
    label_names = trainer.get_label_names()
    print("\nGesture Class Mapping:")
    for idx, name in label_names.items():
        print(f"{idx}: {name}")
    
    print("\nTraining demo completed!")


def demo_prediction():
    """演示模型预测过程"""
    print("\n" + "="*60)
    print("GESTURE RECOGNITION PREDICTION DEMO")
    print("="*60)
    
    try:
        # 创建预测器实例
        predictor = GesturePredictor()
        
        # 获取测试文件
        test_files = []
        for root, dirs, files in os.walk("test"):
            for file in files:
                if file.endswith(".csv"):
                    test_files.append(os.path.join(root, file))
        
        if test_files:
            print(f"\nFound {len(test_files)} test files")
            print("Making predictions on test files...")
            
            for i, test_file in enumerate(test_files[:3]):  # 只测试前3个文件
                try:
                    predicted_label, confidence, all_probs = predictor.predict_from_csv(test_file)
                    print(f"\nTest File {i+1}: {os.path.basename(test_file)}")
                    print(f"Predicted gesture: {predicted_label}")
                    print(f"Confidence: {confidence:.4f}")
                    
                    # 显示前3个最高概率的类别
                    top3_indices = np.argsort(all_probs)[-3:][::-1]
                    print("Top 3 predictions:")
                    for j, idx in enumerate(top3_indices):
                        print(f"  {j+1}. {predictor.label_names[idx]}: {all_probs[idx]:.4f}")
                        
                except Exception as e:
                    print(f"Error predicting {test_file}: {str(e)}")
        else:
            print("No test files found, demonstrating with simulated data")
            
            # 使用模拟数据演示预测功能
            sample_data = np.random.random((100, 6)).astype(np.float32)  # 100个时间步，6个特征
            predicted_label, confidence, all_probs = predictor.predict_from_raw_data(sample_data)
            print(f"Predicted gesture: {predicted_label}, Confidence: {confidence:.4f}")
    
    except FileNotFoundError:
        print("Model files not found. Please run training first.")
        return
    
    print("\nPrediction demo completed!")


def demo_real_time_prediction():
    """演示实时预测的概念（模拟）"""
    print("\n" + "="*60)
    print("REAL-TIME PREDICTION CONCEPT DEMO")
    print("="*60)
    
    try:
        predictor = GesturePredictor()
        
        print("\nSimulating real-time gesture recognition...")
        print("In a real application, this would receive live sensor data")
        
        # 模拟连续的数据流
        for i in range(5):
            # 模拟从传感器接收的数据块
            # 在实际应用中，这些数据会来自陀螺仪和加速度计
            simulated_sensor_data = np.random.random((100, 6)).astype(np.float32) * 2 - 1  # 范围 [-1, 1]
            
            predicted_label, confidence, _ = predictor.predict_from_raw_data(simulated_sensor_data)
            
            print(f"Frame {i+1}: Predicted '{predicted_label}' with confidence {confidence:.4f}")
        
        print("\nReal-time prediction concept demo completed!")
        
    except FileNotFoundError:
        print("Model files not found. Please run training first.")


def show_dataset_info():
    """显示数据集信息"""
    print("\n" + "="*60)
    print("DATASET INFORMATION")
    print("="*60)
    
    if os.path.exists("dataset"):
        gesture_classes = os.listdir("dataset")
        print(f"Found {len(gesture_classes)} gesture classes:")
        
        total_samples = 0
        for gesture_class in gesture_classes:
            gesture_path = os.path.join("dataset", gesture_class)
            if os.path.isdir(gesture_path):
                files = [f for f in os.listdir(gesture_path) if f.endswith('.csv')]
                print(f"  - {gesture_class}: {len(files)} samples")
                total_samples += len(files)
        
        print(f"\nTotal samples in dataset: {total_samples}")
        
        # 显示一个示例CSV文件的结构
        for gesture_class in gesture_classes:
            gesture_path = os.path.join("dataset", gesture_class)
            if os.path.isdir(gesture_path):
                csv_files = [f for f in os.listdir(gesture_path) if f.endswith('.csv')]
                if csv_files:
                    example_file = os.path.join(gesture_path, csv_files[0])
                    df = pd.read_csv(example_file)
                    print(f"\nExample CSV structure ({os.path.basename(example_file)}):")
                    print(f"  Shape: {df.shape}")
                    print(f"  Columns: {list(df.columns)}")
                    print(f"  Sample data (first 3 rows):")
                    print(df.head(3))
                    break
    else:
        print("Dataset directory not found!")


def main():
    """主函数 - 运行所有演示"""
    print("GYROSCOPE GESTURE RECOGNITION SYSTEM")
    print("=====================================")
    
    # 显示数据集信息
    show_dataset_info()
    
    # 运行演示
    demo_prediction()  # 先运行预测演示，因为可能已经有训练好的模型
    
    demo_real_time_prediction()
    
    print("\n" + "="*60)
    print("DEMO COMPLETE")
    print("For training a new model, run: python final_training.py")
    print("For making predictions, run: python enhanced_predictor.py")
    print("="*60)


if __name__ == "__main__":
    main()