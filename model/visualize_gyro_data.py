import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def visualize_gyro_data(file_path):
    """
    可视化陀螺仪数据
    文件包含列：seq, ax, ay, az, gx, gy, gz
    其中 ax, ay, az 是加速度计数据
    gx, gy, gz 是陀螺仪数据
    """
    # 读取数据
    df = pd.read_csv(file_path)
    
    print(f"数据形状: {df.shape}")
    print(f"列名: {list(df.columns)}")
    print(f"前几行数据:")
    print(df.head())
    
    # 创建一个包含所有数据的图形
    plt.figure(figsize=(14, 10))
    
    # 绘制加速度计数据
    plt.plot(df.index, df['ax'], label='ax (X-axis acceleration)', color='red', alpha=0.7)
    plt.plot(df.index, df['ay'], label='ay (Y-axis acceleration)', color='green', alpha=0.7)
    plt.plot(df.index, df['az'], label='az (Z-axis acceleration)', color='blue', alpha=0.7)
    
    # 绘制陀螺仪数据（使用不同的线型以便区分）
    plt.plot(df.index, df['gx'], label='gx (X-axis rotation)', color='orange', alpha=0.7, linestyle='--')
    plt.plot(df.index, df['gy'], label='gy (Y-axis rotation)', color='purple', alpha=0.7, linestyle='--')
    plt.plot(df.index, df['gz'], label='gz (Z-axis rotation)', color='brown', alpha=0.7, linestyle='--')
    
    plt.title('Accelerometer and Gyroscope Data Over Time')
    plt.xlabel('Sample Index')
    plt.ylabel('Value')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    # 为了更好地观察不同范围的数据，也创建一个分开展示的图
    fig, axes = plt.subplots(2, 1, figsize=(14, 10))
    
    # 上图：加速度计数据
    axes[0].plot(df.index, df['ax'], label='ax (X-axis acceleration)', color='red', alpha=0.7)
    axes[0].plot(df.index, df['ay'], label='ay (Y-axis acceleration)', color='green', alpha=0.7)
    axes[0].plot(df.index, df['az'], label='az (Z-axis acceleration)', color='blue', alpha=0.7)
    axes[0].set_title('Accelerometer Data (ax, ay, az)')
    axes[0].set_xlabel('Sample Index')
    axes[0].set_ylabel('Acceleration Value')
    axes[0].legend()
    axes[0].grid(True, linestyle='--', alpha=0.3)
    
    # 下图：陀螺仪数据
    axes[1].plot(df.index, df['gx'], label='gx (X-axis rotation)', color='orange', alpha=0.7)
    axes[1].plot(df.index, df['gy'], label='gy (Y-axis rotation)', color='purple', alpha=0.7)
    axes[1].plot(df.index, df['gz'], label='gz (Z-axis rotation)', color='brown', alpha=0.7)
    axes[1].set_title('Gyroscope Data (gx, gy, gz)')
    axes[1].set_xlabel('Sample Index')
    axes[1].set_ylabel('Angular Velocity Value')
    axes[1].legend()
    axes[1].grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    # 使用提供的数据文件路径
    file_path = 'triangle/gyro_data_20260221_153858.csv'
    visualize_gyro_data(file_path)