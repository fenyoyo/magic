import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from tensorflow.keras.callbacks import EarlyStopping
from tensorflow import keras
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from time_normalization import time_normalize_sequence

DATASET_PATH = "dataset"
TIME_STEPS = 100

# 读取数据
X = []
y = []
label_map = {}

label_index = 0

for label_name in os.listdir(DATASET_PATH):
    label_path = os.path.join(DATASET_PATH, label_name)

    if not os.path.isdir(label_path):
        continue

    label_map[label_name] = label_index

    for file in os.listdir(label_path):
        df = pd.read_csv(os.path.join(label_path, file))

        data = df[['ax','ay','az','gx','gy','gz']].values

        # 使用线性插值进行时间归一化，不管动作多快多慢，都压缩/拉伸到固定长度
        data = time_normalize_sequence(data, TIME_STEPS)

        X.append(data)
        y.append(label_index)

    label_index += 1

X = np.array(X)
y = np.array(y)

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, stratify=y
)

scaler = StandardScaler()

# 只用训练集 fit
X_train_reshaped = X_train.reshape(-1,6)
scaler.fit(X_train_reshaped)

# 分别 transform
X_train = scaler.transform(X_train_reshaped).reshape(-1,TIME_STEPS,6)
X_test = scaler.transform(X_test.reshape(-1,6)).reshape(-1,TIME_STEPS,6)

# 建立模型
model = tf.keras.Sequential([
    # tf.keras.layers.Input(shape=(TIME_STEPS,6)),
    #
    # tf.keras.layers.Conv1D(8,5,activation='relu'),
    # tf.keras.layers.MaxPooling1D(),
    # tf.keras.layers.Dropout(0.3),
    #
    # tf.keras.layers.Conv1D(16,3,activation='relu'),
    # tf.keras.layers.MaxPooling1D(),
    # tf.keras.layers.Dropout(0.3),
    #
    # tf.keras.layers.Flatten(),
    #
    # tf.keras.layers.Dense(64,activation='relu'),
    # tf.keras.layers.Dropout(0.3),
    # tf.keras.layers.Dense(len(label_map),activation='softmax')  # 包含所有已知类别

    tf.keras.layers.Input(shape=(TIME_STEPS, 6)),

    tf.keras.layers.Conv1D(16, 5, activation='relu'),
    tf.keras.layers.MaxPooling1D(),
    # tf.keras.layers.Dropout(0.3),
    tf.keras.layers.Conv1D(32, 3, activation='relu'),

    tf.keras.layers.GlobalAveragePooling1D(),

    tf.keras.layers.Dense(32, activation='relu'),
    tf.keras.layers.Dense(len(label_map), activation='softmax')
])

model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

model.summary()
early_stop = EarlyStopping(
    monitor='val_loss',
    patience=5,
    restore_best_weights=True
)

model.fit(X_train,y_train,epochs=30,validation_data=(X_test,y_test),callbacks=[early_stop])

# 评估模型
print("\n" + "="*60)
print("评估最终模型...")
print("="*60)
test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
print(f"最终测试准确率：{test_accuracy:.4f}")
print(f"最终测试损失：{test_loss:.4f}")

# 详细评估 - 每个类别的准确率
print("\n" + "="*60)
print("各类别详细评估")
print("="*60)

from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import seaborn as sns

# 获取预测结果
y_pred_probs = model.predict(X_test, verbose=0)
y_pred = np.argmax(y_pred_probs, axis=1)

# 反转标签映射
idx_to_label = {idx: label for label, idx in label_map.items()}
label_names = [idx_to_label[i] for i in range(len(label_map))]

# 打印分类报告
print("\n分类报告:")
print(classification_report(y_test, y_pred, target_names=label_names))

# 计算每个类别的准确率
print("\n各类别准确率:")
for i, label_name in enumerate(label_names):
    mask = y_test == i
    if np.sum(mask) > 0:
        class_acc = np.mean(y_pred[mask] == i)
        print(f"  {label_name}: {class_acc:.4f} ({np.sum(mask)} 个样本)")

# 混淆矩阵
print("\n混淆矩阵:")
cm = confusion_matrix(y_test, y_pred)
print(cm)

# 保存混淆矩阵图
plt.figure(figsize=(10, 8))
sns.heatmap(cm, annot=True, fmt='d', cmap='Blues', 
            xticklabels=label_names, yticklabels=label_names)
plt.title('Confusion Matrix')
plt.ylabel('True Label')
plt.xlabel('Predicted Label')
plt.tight_layout()
plt.savefig('./confusion_matrix.png', dpi=150)
print("\n混淆矩阵图已保存为：confusion_matrix.png")

# 保存训练结果参考数据
results_data = {
    'label_map': label_map,
    'test_accuracy': float(test_accuracy),
    'test_loss': float(test_loss),
    'per_class_accuracy': {},
    'confusion_matrix': cm.tolist()
}

for i, label_name in enumerate(label_names):
    mask = y_test == i
    if np.sum(mask) > 0:
        class_acc = np.mean(y_pred[mask] == i)
        results_data['per_class_accuracy'][label_name] = float(class_acc)

import json
with open('./training_results.json', 'w', encoding='utf-8') as f:
    json.dump(results_data, f, indent=2, ensure_ascii=False)
print("训练结果已保存为：training_results.json")

# 保存模型和预处理器
model.save('./model.h5')
np.save('./scaler_mean.npy', scaler.mean_)
np.save('./scaler_scale.npy', scaler.scale_)
with open('./label_map.txt', 'w') as f:
    for label, idx in label_map.items():
        f.write(f"{label}:{idx}\n")

print("模型和预处理器已保存完成！")
