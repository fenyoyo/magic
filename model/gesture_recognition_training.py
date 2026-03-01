import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv1D, MaxPooling1D, GlobalAveragePooling1D, Dense, Dropout, LSTM
from tensorflow.keras.utils import to_categorical
from tensorflow.keras.callbacks import EarlyStopping, ReduceLROnPlateau
import matplotlib.pyplot as plt
import seaborn as sns
from time_normalization import time_normalize_sequence


class GestureRecognitionTrainer:
    def __init__(self, dataset_path="dataset", target_length=100):
        self.dataset_path = dataset_path
        self.target_length = target_length
        self.scaler = StandardScaler()
        self.label_encoder = LabelEncoder()
        self.model = None
        self.history = None
        
    def load_data(self):
        """从数据集中加载陀螺仪数据"""
        print("正在加载数据集...")
        
        X = []
        y = []
        
        # 遍历数据集目录
        for gesture_class in os.listdir(self.dataset_path):
            gesture_path = os.path.join(self.dataset_path, gesture_class)
            
            if not os.path.isdir(gesture_path):
                continue
                -
            print(f"正在处理手势类别: {gesture_class}")
            
            # 遍历该类别的所有CSV文件
            for csv_file in os.listdir(gesture_path):
                if csv_file.endswith('.csv'):
                    csv_path = os.path.join(gesture_path, csv_file)
                    
                    try:
                        # 读取CSV文件
                        df = pd.read_csv(csv_path)
                        
                        # 提取陀螺仪和加速度计数据
                        if all(col in df.columns for col in ['ax', 'ay', 'az', 'gx', 'gy', 'gz']):
                            # 提取数据
                            data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values.astype(np.float32)
                            
                            # 时间归一化
                            data = time_normalize_sequence(data, self.target_length)
                            
                            X.append(data)
                            y.append(gesture_class)
                        else:
                            print(f"警告: {csv_file} 缺少必要的列")
                    except Exception as e:
                        print(f"错误: 读取 {csv_file} 时出现问题: {str(e)}")
        
        if len(X) == 0:
            raise ValueError("没有找到有效的数据文件，请检查数据集路径和文件格式")
        
        X = np.array(X)
        y = np.array(y)
        
        print(f"总共加载了 {len(X)} 个样本，包含 {len(set(y))} 个手势类别")
        
        # 对标签进行编码
        y_encoded = self.label_encoder.fit_transform(y)
        
        return X, y_encoded
    
    def preprocess_data(self, X, y):
        """预处理数据"""
        print("正在预处理数据...")
        
        # 重塑数据用于标准化
        X_reshaped = X.reshape(-1, X.shape[-1])
        
        # 拟合并转换数据
        X_scaled = self.scaler.fit_transform(X_reshaped)
        X_scaled = X_scaled.reshape(X.shape)
        
        # 将标签转换为分类格式
        num_classes = len(np.unique(y))
        y_categorical = to_categorical(y, num_classes=num_classes)
        
        return X_scaled, y_categorical
    
    def build_model(self, input_shape, num_classes):
        """构建CNN-LSTM混合模型"""
        print("正在构建模型...")
        
        model = Sequential([
            # 第一层卷积
            Conv1D(filters=32, kernel_size=3, activation='relu', input_shape=input_shape),
            Conv1D(filters=32, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Dropout(0.25),
            
            # 第二层卷积
            Conv1D(filters=64, kernel_size=3, activation='relu'),
            Conv1D(filters=64, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Dropout(0.25),
            
            # LSTM层捕获时序特征
            LSTM(50, return_sequences=True),
            Dropout(0.2),
            
            LSTM(50),
            Dropout(0.2),
            
            # 全连接层
            Dense(128, activation='relu'),
            Dropout(0.5),
            
            Dense(64, activation='relu'),
            Dropout(0.3),
            
            # 输出层
            Dense(num_classes, activation='softmax')
        ])
        
        model.compile(
            optimizer='adam',
            loss='categorical_crossentropy',
            metrics=['accuracy']
        )
        
        self.model = model
        return model
    
    def train(self, epochs=100, validation_split=0.2, batch_size=32):
        """训练模型"""
        print("开始训练模型...")
        
        # 加载数据
        X, y = self.load_data()
        
        # 预处理数据
        X_processed, y_processed = self.preprocess_data(X, y)
        
        # 划分训练集和测试集
        X_train, X_test, y_train, y_test = train_test_split(
            X_processed, y_processed, test_size=0.2, random_state=42, stratify=y_processed.argmax(axis=1)
        )
        
        print(f"训练集大小: {X_train.shape[0]}, 测试集大小: {X_test.shape[0]}")
        
        # 构建模型
        input_shape = (X_train.shape[1], X_train.shape[2])
        num_classes = y_train.shape[1]
        
        model = self.build_model(input_shape, num_classes)
        
        # 定义回调函数
        callbacks = [
            EarlyStopping(monitor='val_loss', patience=15, restore_best_weights=True),
            ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=7, min_lr=1e-7)
        ]
        
        # 训练模型
        self.history = model.fit(
            X_train, y_train,
            validation_data=(X_test, y_test),
            epochs=epochs,
            batch_size=batch_size,
            callbacks=callbacks,
            verbose=1
        )
        
        # 评估模型
        test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
        print(f"测试准确率: {test_accuracy:.4f}")
        
        return model, self.history
    
    def plot_training_history(self):
        """绘制训练历史"""
        if self.history is None:
            print("没有训练历史可绘制")
            return
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
        
        # 绘制准确率
        ax1.plot(self.history.history['accuracy'], label='训练准确率')
        ax1.plot(self.history.history['val_accuracy'], label='验证准确率')
        ax1.set_title('模型准确率')
        ax1.set_xlabel('Epoch')
        ax1.set_ylabel('Accuracy')
        ax1.legend()
        ax1.grid(True)
        
        # 绘制损失
        ax2.plot(self.history.history['loss'], label='训练损失')
        ax2.plot(self.history.history['val_loss'], label='验证损失')
        ax2.set_title('模型损失')
        ax2.set_xlabel('Epoch')
        ax2.set_ylabel('Loss')
        ax2.legend()
        ax2.grid(True)
        
        plt.tight_layout()
        plt.show()
    
    def get_label_names(self):
        """获取标签名称映射"""
        return dict(zip(range(len(self.label_encoder.classes_)), self.label_encoder.classes_))


def main():
    # 创建训练器实例
    trainer = GestureRecognitionTrainer(dataset_path="dataset", target_length=100)
    
    # 训练模型
    model, history = trainer.train(epochs=100, batch_size=16)
    
    # 绘制训练历史
    trainer.plot_training_history()
    
    # 显示标签映射
    label_names = trainer.get_label_names()
    print("\n手势类别映射:")
    for idx, name in label_names.items():
        print(f"{idx}: {name}")
    
    # 保存模型
    model.save("gesture_recognition_model.h5")
    print("\n模型已保存为 'gesture_recognition_model.h5'")
    
    # 保存预处理器
    import joblib
    joblib.dump(trainer.scaler, "scaler.pkl")
    joblib.dump(trainer.label_encoder, "label_encoder.pkl")
    print("预处理器已保存")


if __name__ == "__main__":
    main()