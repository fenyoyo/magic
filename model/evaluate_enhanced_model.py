import numpy as np
import pandas as pd
import tensorflow as tf
import joblib
import os
import glob
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import seaborn as sns


def load_gyro_data(file_path):
    """
    从CSV文件加载陀螺仪数据
    """
    df = pd.read_csv(file_path)
    # 使用陀螺仪数据列: gx, gy, gz, ax, ay, az
    columns_to_use = ['gx', 'gy', 'gz', 'ax', 'ay', 'az']
    # 确保这些列存在
    available_columns = [col for col in columns_to_use if col in df.columns]
    if len(available_columns) < 6:
        raise ValueError(f"文件 {file_path} 中缺少足够的陀螺仪数据列")
    
    data = df[available_columns].values
    return data


def pad_or_truncate_sequence(sequence, target_length):
    """
    将序列填充或截断到目标长度
    """
    if sequence.shape[0] < target_length:
        # 如果序列较短，则用零填充
        padded_seq = np.zeros((target_length, sequence.shape[1]))
        padded_seq[:sequence.shape[0], :] = sequence
        return padded_seq
    elif sequence.shape[0] > target_length:
        # 如果序列较长，则截断
        return sequence[:target_length, :]
    else:
        return sequence


def predict_shape(model, scaler, file_path, sequence_length=250):
    """
    对单个文件进行预测
    """
    # 加载数据
    data = load_gyro_data(file_path)
    
    # 预处理
    processed_data = pad_or_truncate_sequence(data, sequence_length)
    processed_data = processed_data.reshape(1, sequence_length, 6)  # 添加批次维度
    
    # 标准化
    original_shape = processed_data.shape
    processed_data = processed_data.reshape(-1, processed_data.shape[-1])
    processed_data = scaler.transform(processed_data)
    processed_data = processed_data.reshape(original_shape)
    
    # 预测
    prediction = model.predict(processed_data)
    predicted_class = np.argmax(prediction[0])
    confidence = np.max(prediction[0])
    
    return predicted_class, confidence


def evaluate_model_on_test_set(model, scaler, data_dir, sequence_length=250):
    """
    在整个测试集上评估模型
    """
    # 获取所有CSV文件
    csv_files = glob.glob(os.path.join(data_dir, "*.csv"))
    
    true_labels = []
    pred_labels = []
    confidences = []
    
    class_names = ['Triangle', 'Circle', 'Square', 'Random']
    
    for file_path in csv_files:
        # 从文件名提取真实标签
        filename = os.path.basename(file_path)
        if filename.startswith('triangle'):
            true_label = 0
        elif filename.startswith('circle'):
            true_label = 1
        elif filename.startswith('square'):
            true_label = 2
        elif filename.startswith('random'):
            true_label = 3
        else:
            continue
        
        try:
            predicted_label, confidence = predict_shape(model, scaler, file_path, sequence_length)
            
            true_labels.append(true_label)
            pred_labels.append(predicted_label)
            confidences.append(confidence)
            
            print(f"文件: {filename}, 真实: {class_names[true_label]}, 预测: {class_names[predicted_label]}, 置信度: {confidence:.3f}")
        except Exception as e:
            print(f"处理文件 {file_path} 时出错: {e}")
            continue
    
    # 计算整体准确率
    true_labels = np.array(true_labels)
    pred_labels = np.array(pred_labels)
    
    accuracy = np.mean(true_labels == pred_labels)
    print(f"\n整体准确率: {accuracy:.3f}")
    
    # 打印分类报告
    class_names_display = ['Triangle', 'Circle', 'Square', 'Random']
    print("\n分类报告:")
    print(classification_report(true_labels, pred_labels, target_names=class_names_display))
    
    # 绘制混淆矩阵
    cm = confusion_matrix(true_labels, pred_labels)
    plt.figure(figsize=(8, 6))
    sns.heatmap(cm, annot=True, fmt='d', cmap='Blues', 
                xticklabels=class_names_display, 
                yticklabels=class_names_display)
    plt.title('Confusion Matrix')
    plt.xlabel('Predicted Label')
    plt.ylabel('True Label')
    plt.tight_layout()
    plt.savefig('confusion_matrix_enhanced.png')
    plt.show()
    
    return accuracy, true_labels, pred_labels, confidences


def main():
    # 检查增强模型是否存在
    model_path = 'gyro_shape_classifier_enhanced.h5'
    scaler_path = 'scaler_enhanced.pkl'
    
    if not os.path.exists(model_path) or not os.path.exists(scaler_path):
        print("未找到增强模型，尝试使用原模型...")
        model_path = 'gyro_shape_classifier_improved.h5'
        scaler_path = 'scaler_improved.pkl'
    
    if not os.path.exists(model_path) or not os.path.exists(scaler_path):
        print("错误: 未找到任何训练好的模型文件")
        return
    
    # 加载训练好的模型
    print("加载模型...")
    model = tf.keras.models.load_model(model_path)
    
    # 加载标准化器
    print("加载标准化器...")
    scaler = joblib.load(scaler_path)
    
    print("模型和标准化器加载完成")
    print(f"使用模型: {model_path}")
    
    # 在测试集上评估模型
    print("\n开始评估模型...")
    accuracy, true_labels, pred_labels, confidences = evaluate_model_on_test_set(model, scaler, 'data2')
    
    # 测试单个文件预测
    print("\n测试单个文件预测功能...")
    test_files = glob.glob(os.path.join('data2', "triangle_*.csv"))[:3]  # 选择前3个三角形文件测试
    
    class_names = ['Triangle', 'Circle', 'Square', 'Random']
    
    for file_path in test_files:
        predicted_class, confidence = predict_shape(model, scaler, file_path)
        filename = os.path.basename(file_path)
        print(f"文件: {filename} -> 预测: {class_names[predicted_class]}, 置信度: {confidence:.3f}")
    
    # 加载TFLite模型进行比较
    tflite_path = model_path.replace('.h5', '.tflite')
    if os.path.exists(tflite_path):
        print("\n加载TFLite模型进行比较...")
        interpreter = tf.lite.Interpreter(model_path=tflite_path)
        interpreter.allocate_tensors()
        
        # 获取输入和输出张量
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        
        # 测试一个示例
        sample_file = test_files[0]
        sample_data = load_gyro_data(sample_file)
        sample_data = pad_or_truncate_sequence(sample_data, 250)
        sample_data = sample_data.reshape(1, 250, 6)
        
        # 标准化
        original_shape = sample_data.shape
        sample_data_flat = sample_data.reshape(-1, sample_data.shape[-1])
        sample_data_flat = scaler.transform(sample_data_flat)
        sample_data = sample_data_flat.reshape(original_shape).astype(np.float32)
        
        # TFLite推理
        interpreter.set_tensor(input_details[0]['index'], sample_data)
        interpreter.invoke()
        tflite_prediction = interpreter.get_tensor(output_details[0]['index'])
        
        predicted_class_tflite = np.argmax(tflite_prediction[0])
        confidence_tflite = np.max(tflite_prediction[0])
        
        print(f"TFLite模型预测 - 文件: {os.path.basename(sample_file)} -> 预测: {class_names[predicted_class_tflite]}, 置信度: {confidence_tflite:.3f}")
    
    print(f"\n增强版TinyML陀螺仪形状分类器部署就绪!")
    print(f"完整模型大小: {os.path.getsize(model_path) / 1024:.2f} KB")
    if os.path.exists(tflite_path):
        print(f"TFLite模型大小: {os.path.getsize(tflite_path) / 1024:.2f} KB")


if __name__ == '__main__':
    main()