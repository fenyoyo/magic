import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os
import glob
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import matplotlib.pyplot as plt


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


def prepare_data(data_dir, max_seq_length=250):
    """
    准备训练数据
    """
    # 获取所有CSV文件
    csv_files = glob.glob(os.path.join(data_dir, "*.csv"))
    
    # 根据文件名确定标签
    X = []
    y = []
    
    for file_path in csv_files:
        # 从文件名提取标签
        filename = os.path.basename(file_path)
        if filename.startswith('triangle'):
            label = 0  # 三角形
        elif filename.startswith('circle'):
            label = 1  # 圆形
        elif filename.startswith('square'):
            label = 2  # 正方形
        elif filename.startswith('random'):
            label = 3  # 随机
        else:
            continue  # 跳过未知类型
        
        try:
            data = load_gyro_data(file_path)
            # 预处理数据
            processed_data = pad_or_truncate_sequence(data, max_seq_length)
            X.append(processed_data)
            y.append(label)
        except Exception as e:
            print(f"处理文件 {file_path} 时出错: {e}")
            continue
    
    X = np.array(X)
    y = np.array(y)
    
    return X, y


def create_enhanced_model(input_shape, num_classes=4):
    """
    创建增强的用于陀螺仪数据分类的CNN-LSTM混合模型
    """
    model = keras.Sequential([
        layers.Input(shape=input_shape),
        
        # 第一个卷积块 - 提取局部特征
        layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.Dropout(0.2),
        
        layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.MaxPooling1D(pool_size=2),
        layers.Dropout(0.3),
        
        # 第二个卷积块
        layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.Dropout(0.3),
        
        layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.MaxPooling1D(pool_size=2),
        layers.Dropout(0.4),
        
        # 第三个卷积块
        layers.Conv1D(128, kernel_size=3, activation='relu', padding='same'),
        layers.BatchNormalization(),
        layers.Dropout(0.4),
        
        # LSTM层 - 捕获长期依赖关系
        layers.LSTM(64, return_sequences=True, dropout=0.3),
        layers.LSTM(32, return_sequences=False, dropout=0.3),
        
        # 全连接层
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.5),
        layers.Dense(32, activation='relu'),
        layers.Dropout(0.4),
        
        # 输出层
        layers.Dense(num_classes, activation='softmax')
    ])
    
    return model


def augment_data(X, y, augmentation_factor=2):
    """
    数据增强 - 增加数据多样性
    """
    X_aug = []
    y_aug = []
    
    for i in range(len(X)):
        X_aug.append(X[i])
        y_aug.append(y[i])
        
        # 添加噪声增强
        for _ in range(augmentation_factor):
            augmented_sample = X[i].copy()
            # 添加高斯噪声
            noise = np.random.normal(0, 0.05, size=augmented_sample.shape)
            augmented_sample += noise
            # 添加小的随机偏移
            offset = np.random.uniform(-0.1, 0.1, size=(1, augmented_sample.shape[1]))
            augmented_sample += offset
            X_aug.append(augmented_sample)
            y_aug.append(y[i])
    
    return np.array(X_aug), np.array(y_aug)


def main():
    print("开始准备数据...")
    
    # 准备数据
    X, y = prepare_data('data', max_seq_length=250)
    
    print(f"原始数据形状: X={X.shape}, y={y.shape}")
    print(f"原始类别分布: {np.bincount(y)}")
    
    # 数据增强
    print("进行数据增强...")
    X, y = augment_data(X, y, augmentation_factor=3)
    print(f"增强后数据形状: X={X.shape}, y={y.shape}")
    print(f"增强后类别分布: {np.bincount(y)}")
    
    # 划分训练集和测试集
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )
    
    # 标准化数据
    # 将3D数组重塑为2D以进行标准化
    X_train_flat = X_train.reshape(-1, X_train.shape[-1])
    X_test_flat = X_test.reshape(-1, X_test.shape[-1])
    
    scaler = StandardScaler()
    X_train_flat = scaler.fit_transform(X_train_flat)
    X_test_flat = scaler.transform(X_test_flat)
    
    # 重新整形回3D
    X_train = X_train_flat.reshape(X_train.shape)
    X_test = X_test_flat.reshape(X_test.shape)
    
    print("数据预处理完成")
    
    # 创建模型
    input_shape = (X_train.shape[1], X_train.shape[2])  # (sequence_length, features)
    model = create_enhanced_model(input_shape, num_classes=4)
    
    # 编译模型
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    
    print("模型架构:")
    model.summary()
    
    # 定义回调函数
    callbacks = [
        keras.callbacks.EarlyStopping(
            monitor='val_accuracy',
            patience=15,
            restore_best_weights=True,
            verbose=1
        ),
        keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=8,
            min_lr=1e-7,
            verbose=1
        ),
        keras.callbacks.ModelCheckpoint(
            'best_gyro_model.h5',
            monitor='val_accuracy',
            save_best_only=True,
            verbose=1
        )
    ]
    
    # 训练模型
    print("开始训练模型...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_test, y_test),
        epochs=100,  # 增加epoch数量，配合early stopping
        batch_size=16,  # 减小batch size以提高稳定性
        callbacks=callbacks,
        verbose=1
    )
    
    # 评估模型
    print("评估最终模型...")
    test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
    print(f"\n最终测试准确率: {test_accuracy:.4f}")
    
    # 加载最佳模型（如果有保存的话）
    if os.path.exists('best_gyro_model.h5'):
        print("加载最佳模型...")
        model = keras.models.load_model('best_gyro_model.h5')
        test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
        print(f"最佳模型测试准确率: {test_accuracy:.4f}")
    
    # 保存最终模型
    model.save('gyro_shape_classifier_enhanced.h5')
    print("模型已保存为 'gyro_shape_classifier_enhanced.h5'")
    
    # 保存标准化器
    import joblib
    joblib.dump(scaler, 'scaler_enhanced.pkl')
    print("标准化器已保存为 'scaler_enhanced.pkl'")
    
    # 绘制训练历史
    plt.figure(figsize=(12, 4))
    
    plt.subplot(1, 2, 1)
    plt.plot(history.history['accuracy'], label='Training Accuracy')
    plt.plot(history.history['val_accuracy'], label='Validation Accuracy')
    plt.title('Model Accuracy')
    plt.xlabel('Epoch')
    plt.ylabel('Accuracy')
    plt.legend()
    
    plt.subplot(1, 2, 2)
    plt.plot(history.history['loss'], label='Training Loss')
    plt.plot(history.history['val_loss'], label='Validation Loss')
    plt.title('Model Loss')
    plt.xlabel('Epoch')
    plt.ylabel('Loss')
    plt.legend()
    
    plt.tight_layout()
    plt.savefig('training_history_enhanced.png')
    plt.show()
    
    # 转换为TensorFlow Lite模型（TinyML优化）
    print("转换为TensorFlow Lite模型...")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]  # 量化优化
    # 启用更积极的量化策略
    converter.representative_dataset = lambda: [X_test[:100].astype(np.float32)]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite_model = converter.convert()
    
    # 保存TFLite模型
    with open('gyro_shape_classifier_enhanced.tflite', 'wb') as f:
        f.write(tflite_model)
    
    print("TensorFlow Lite模型已保存为 'gyro_shape_classifier_enhanced.tflite'")
    
    # 显示模型大小信息
    original_size = os.path.getsize('gyro_shape_classifier_enhanced.h5')
    tflite_size = os.path.getsize('gyro_shape_classifier_enhanced.tflite')
    
    print(f"原始模型大小: {original_size / 1024:.2f} KB")
    print(f"TFLite模型大小: {tflite_size / 1024:.2f} KB")


if __name__ == '__main__':
    main()