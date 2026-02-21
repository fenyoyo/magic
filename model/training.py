import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
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

# 归一化
scaler = StandardScaler()
X_reshaped = X.reshape(-1,6)
X_scaled = scaler.fit_transform(X_reshaped)
X = X_scaled.reshape(-1,TIME_STEPS,6)

# 划分数据
X_train, X_test, y_train, y_test = train_test_split(X,y,test_size=0.2)

# 建立模型
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(TIME_STEPS,6)),

    tf.keras.layers.Conv1D(16,5,activation='relu'),
    tf.keras.layers.MaxPooling1D(),

    tf.keras.layers.Conv1D(32,3,activation='relu'),
    tf.keras.layers.MaxPooling1D(),

    tf.keras.layers.Flatten(),

    tf.keras.layers.Dense(64,activation='relu'),
    tf.keras.layers.Dense(len(label_map),activation='softmax')  # 包含所有已知类别
])

model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

model.summary()

model.fit(X_train,y_train,epochs=30,validation_data=(X_test,y_test))

# 保存模型和预处理器
model.save('./output/model.h5')
np.save('./output/scaler_mean.npy', scaler.mean_)
np.save('./output/scaler_scale.npy', scaler.scale_)
with open('./output/label_map.txt', 'w') as f:
    for label, idx in label_map.items():
        f.write(f"{label}:{idx}\n")

print("模型和预处理器已保存完成！")
