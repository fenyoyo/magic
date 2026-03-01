import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.model_selection import train_test_split, StratifiedKFold
from sklearn.preprocessing import StandardScaler, LabelEncoder
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv1D, MaxPooling1D, GlobalAveragePooling1D, Dense, Dropout, LSTM, Input
from tensorflow.keras.utils import to_categorical
from tensorflow.keras.callbacks import EarlyStopping, ReduceLROnPlateau, ModelCheckpoint
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import seaborn as sns
from time_normalization import time_normalize_sequence


class GestureRecognitionTrainer:
    def __init__(self, dataset_path="dataset", target_length=100, validation_split=0.2):
        self.dataset_path = dataset_path
        self.target_length = target_length
        self.validation_split = validation_split
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
        """构建CNN-LSTM混合模型 - 已优化架构以防止过拟合"""
        print("正在构建模型...")
        
        model = Sequential([
            # 明确定义输入层以避免警告
            Input(shape=input_shape),
            
            # 第一层卷积 - 使用更小的滤波器以减少参数
            Conv1D(filters=16, kernel_size=5, activation='relu'),
            Conv1D(filters=16, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Dropout(0.3),  # 增加dropout率
            
            # 第二层卷积
            Conv1D(filters=32, kernel_size=3, activation='relu'),
            Conv1D(filters=32, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Dropout(0.3),  # 增加dropout率
            
            # 第三层卷积 - 添加以增加深度但控制参数
            Conv1D(filters=64, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Dropout(0.4),  # 更高的dropout率
            
            # LSTM层捕获时序特征 - 减少单元数以减少参数
            LSTM(32, return_sequences=True),
            Dropout(0.3),
            
            LSTM(32),
            Dropout(0.3),
            
            # 全连接层 - 减少神经元数量
            Dense(64, activation='relu'),
            Dropout(0.5),  # 高dropout率
            
            Dense(32, activation='relu'),
            Dropout(0.4),
            
            # 输出层
            Dense(num_classes, activation='softmax')
        ])
        
        model.compile(
            optimizer='adam',
            loss='categorical_crossentropy',
            metrics=['accuracy']
        )
        
        print(f"模型参数总数: {model.count_params():,}")
        
        self.model = model
        return model
    
    def train_with_validation(self, epochs=100, batch_size=16, k_folds=5):
        """使用交叉验证进行训练以检测过拟合"""
        print("开始训练模型...")
        
        # 加载数据
        X, y = self.load_data()
        
        # 预处理数据
        X_processed, y_processed = self.preprocess_data(X, y)
        
        # 划分训练集和测试集 (保留一部分用于最终测试)
        X_temp, X_test, y_temp, y_test = train_test_split(
            X_processed, y_processed, test_size=0.15, random_state=42, 
            stratify=y_processed.argmax(axis=1)
        )
        
        # 从临时数据中划分训练和验证集
        X_train, X_val, y_train, y_val = train_test_split(
            X_temp, y_temp, test_size=0.2, random_state=42,
            stratify=y_temp.argmax(axis=1)
        )
        
        print(f"训练集大小: {X_train.shape[0]}, 验证集大小: {X_val.shape[0]}, 测试集大小: {X_test.shape[0]}")
        
        # 构建模型
        input_shape = (X_train.shape[1], X_train.shape[2])
        num_classes = y_train.shape[1]
        
        model = self.build_model(input_shape, num_classes)
        
        # 定义回调函数 - 更严格的早停策略
        callbacks = [
            EarlyStopping(
                monitor='val_loss', 
                patience=10,  # 减少patience以防止过拟合
                restore_best_weights=True,
                verbose=1
            ),
            ReduceLROnPlateau(
                monitor='val_loss', 
                factor=0.2,  # 更激进的学习率衰减
                patience=5,  # 更快的响应
                min_lr=1e-7, 
                verbose=1
            ),
            ModelCheckpoint(
                'best_model.keras',
                monitor='val_loss',
                save_best_only=True,
                save_weights_only=False,
                verbose=1
            )
        ]
        
        # 训练模型
        self.history = model.fit(
            X_train, y_train,
            validation_data=(X_val, y_val),  # 使用独立的验证集
            epochs=epochs,
            batch_size=batch_size,
            callbacks=callbacks,
            verbose=1
        )
        
        # 在验证集上评估模型
        val_loss, val_accuracy = model.evaluate(X_val, y_val, verbose=0)
        print(f"验证准确率: {val_accuracy:.4f}")
        
        # 在测试集上评估模型 (这部分之前从未见过)
        test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
        print(f"测试准确率: {test_accuracy:.4f}")
        
        # 生成详细的分类报告
        y_pred = model.predict(X_test)
        y_pred_classes = np.argmax(y_pred, axis=1)
        y_true_classes = np.argmax(y_test, axis=1)
        
        print("\n分类报告:")
        print(classification_report(y_true_classes, y_pred_classes, 
                                  target_names=self.label_encoder.classes_))
        
        # 绘制混淆矩阵
        cm = confusion_matrix(y_true_classes, y_pred_classes)
        plt.figure(figsize=(10, 8))
        sns.heatmap(cm, annot=True, fmt='d', cmap='Blues', 
                   xticklabels=self.label_encoder.classes_, 
                   yticklabels=self.label_encoder.classes_)
        plt.title('混淆矩阵')
        plt.xlabel('预测标签')
        plt.ylabel('真实标签')
        plt.show()
        
        # 检查过拟合情况
        train_acc = self.history.history['accuracy']
        val_acc = self.history.history['val_accuracy']
        
        final_train_acc = train_acc[-1]
        final_val_acc = val_acc[-1]
        gap = final_train_acc - final_val_acc
        
        print(f"\n过拟合检测:")
        print(f"最终训练准确率: {final_train_acc:.4f}")
        print(f"最终验证准确率: {final_val_acc:.4f}")
        print(f"准确率差距: {gap:.4f}")
        
        if gap > 0.05:  # 如果差距超过5%，认为存在轻微过拟合
            print("⚠️  检测到可能的过拟合现象")
        elif gap > 0.1:  # 如果差距超过10%，认为存在明显过拟合
            print("🚨 检测到明显的过拟合现象")
        else:
            print("✅ 模型泛化性能良好")
        
        return model, self.history, test_accuracy
    
    def plot_training_history(self):
        """绘制训练历史"""
        if self.history is None:
            print("没有训练历史可绘制")
            return
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
        
        # 绘制准确率
        ax1.plot(self.history.history['accuracy'], label='训练准确率', linewidth=2)
        ax1.plot(self.history.history['val_accuracy'], label='验证准确率', linewidth=2)
        ax1.set_title('模型准确率')
        ax1.set_xlabel('Epoch')
        ax1.set_ylabel('Accuracy')
        ax1.legend()
        ax1.grid(True, linestyle='--', alpha=0.6)
        
        # 绘制损失
        ax2.plot(self.history.history['loss'], label='训练损失', linewidth=2)
        ax2.plot(self.history.history['val_loss'], label='验证损失', linewidth=2)
        ax2.set_title('模型损失')
        ax2.set_xlabel('Epoch')
        ax2.set_ylabel('Loss')
        ax2.legend()
        ax2.grid(True, linestyle='--', alpha=0.6)
        
        plt.tight_layout()
        plt.show()
    
    def get_label_names(self):
        """获取标签名称映射"""
        return dict(zip(range(len(self.label_encoder.classes_)), self.label_encoder.classes_))


def main():
    # 创建训练器实例
    trainer = GestureRecognitionTrainer(dataset_path="dataset", target_length=100)
    
    # 训练模型并进行验证
    model, history, test_accuracy = trainer.train_with_validation(epochs=100, batch_size=16)
    
    # 绘制训练历史
    trainer.plot_training_history()
    
    # 显示标签映射
    label_names = trainer.get_label_names()
    print("\n手势类别映射:")
    for idx, name in label_names.items():
        print(f"{idx}: {name}")
    
    # 保存最佳模型
    model.save("gesture_recognition_model.keras")
    print("\n模型已保存为 'gesture_recognition_model.keras'")
    
    # 保存预处理器
    import joblib
    joblib.dump(trainer.scaler, "scaler.pkl")
    joblib.dump(trainer.label_encoder, "label_encoder.pkl")
    print("预处理器已保存")


if __name__ == "__main__":
    main()