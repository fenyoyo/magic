import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from tensorflow import keras

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

        if len(data) >= TIME_STEPS:
            data = data[:TIME_STEPS]
        else:
            pad = np.zeros((TIME_STEPS-len(data),6))
            data = np.vstack((data,pad))

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
    tf.keras.layers.Dense(len(label_map),activation='softmax')
])

model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

model.summary()

model.fit(X_train,y_train,epochs=30,validation_data=(X_test,y_test))

# 评估模型
print("评估最终模型...")
test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
print(f"\n最终测试准确率: {test_accuracy:.4f}")


def predict_from_csv(model, scaler, label_map, csv_path, time_steps=100):
    """
    从新的CSV文件预测
    """
    # 读取CSV
    df = pd.read_csv(csv_path)

    # 提取需要的列
    data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values

    # 预处理：截断或填充
    if len(data) >= time_steps:
        data = data[:time_steps]
    else:
        pad = np.zeros((time_steps - len(data), 6))
        data = np.vstack((data, pad))

    # 标准化
    data_scaled = scaler.transform(data)

    # 重塑为模型输入格式
    data_reshaped = data_scaled.reshape(1, time_steps, 6)

    # 预测
    predictions = model.predict(data_reshaped, verbose=0)
    predicted_class = np.argmax(predictions[0])
    confidence = np.max(predictions[0])

    # 获取标签名
    label_names = list(label_map.keys())
    label_values = list(label_map.values())
    predicted_label = label_names[label_values.index(predicted_class)]

    # 显示所有类别的概率
    print("\n所有类别概率:")
    for i, prob in enumerate(predictions[0]):
        label = label_names[label_values.index(i)]
        print(f"  {label}: {prob:.4f}")

    return predicted_label, confidence

new_csv = "test/lightning_001.csv"

if os.path.exists(new_csv):
    label, confidence = predict_from_csv(model, scaler, label_map, new_csv)
    print(f"\n预测结果: {label}")
    print(f"置信度: {confidence:.4f}")