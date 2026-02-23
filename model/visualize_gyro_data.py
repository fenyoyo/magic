import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from time_normalization import time_normalize_sequence

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

    # # 创建一个包含所有数据的图形
    # plt.figure(figsize=(14, 10))
    #
    # # 绘制加速度计数据
    # plt.plot(df.index, df['ax'], label='ax (X-axis acceleration)', color='red', alpha=0.7)
    # plt.plot(df.index, df['ay'], label='ay (Y-axis acceleration)', color='green', alpha=0.7)
    # plt.plot(df.index, df['az'], label='az (Z-axis acceleration)', color='blue', alpha=0.7)
    #
    # # 绘制陀螺仪数据（使用不同的线型以便区分）
    # plt.plot(df.index, df['gx'], label='gx (X-axis rotation)', color='orange', alpha=0.7, linestyle='--')
    # plt.plot(df.index, df['gy'], label='gy (Y-axis rotation)', color='purple', alpha=0.7, linestyle='--')
    # plt.plot(df.index, df['gz'], label='gz (Z-axis rotation)', color='brown', alpha=0.7, linestyle='--')
    #
    # plt.title('Accelerometer and Gyroscope Data Over Time')
    # plt.xlabel('Sample Index')
    # plt.ylabel('Value')
    # plt.legend()
    # plt.grid(True, linestyle='--', alpha=0.3)
    #
    # plt.tight_layout()
    # plt.show()

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


def compare_original_and_normalized(file_path, target_length=100):
    """
    对比原始数据和经过time_normalize_sequence处理后的数据
    """
    # 读取原始数据
    df = pd.read_csv(file_path)
    original_data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values
    original_length = len(original_data)
    
    print(f"原始数据长度: {original_length}")
    print(f"目标长度: {target_length}")

    # 应用时间归一化
    normalized_data = time_normalize_sequence(original_data, target_length)
    
    # 创建对比图
    fig, axes = plt.subplots(3, 2, figsize=(16, 12))
    
    # 加速度计数据对比
    axes[0, 0].plot(range(original_length), original_data[:, 0], label='ax', color='red', alpha=0.7)
    axes[0, 0].plot(range(original_length), original_data[:, 1], label='ay', color='green', alpha=0.7)
    axes[0, 0].plot(range(original_length), original_data[:, 2], label='az', color='blue', alpha=0.7)
    axes[0, 0].set_title(f'原始加速度计数据 (长度: {original_length})')
    axes[0, 0].set_xlabel('时间索引')
    axes[0, 0].set_ylabel('加速度值')
    axes[0, 0].legend()
    axes[0, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[0, 1].plot(range(target_length), normalized_data[:, 0], label='ax', color='red', alpha=0.7)
    axes[0, 1].plot(range(target_length), normalized_data[:, 1], label='ay', color='green', alpha=0.7)
    axes[0, 1].plot(range(target_length), normalized_data[:, 2], label='az', color='blue', alpha=0.7)
    axes[0, 1].set_title(f'归一化后加速度计数据 (长度: {target_length})')
    axes[0, 1].set_xlabel('时间索引')
    axes[0, 1].set_ylabel('加速度值')
    axes[0, 1].legend()
    axes[0, 1].grid(True, linestyle='--', alpha=0.3)
    
    # 陀螺仪数据对比
    axes[1, 0].plot(range(original_length), original_data[:, 3], label='gx', color='orange', alpha=0.7)
    axes[1, 0].plot(range(original_length), original_data[:, 4], label='gy', color='purple', alpha=0.7)
    axes[1, 0].plot(range(original_length), original_data[:, 5], label='gz', color='brown', alpha=0.7)
    axes[1, 0].set_title(f'原始陀螺仪数据 (长度: {original_length})')
    axes[1, 0].set_xlabel('时间索引')
    axes[1, 0].set_ylabel('角速度值')
    axes[1, 0].legend()
    axes[1, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[1, 1].plot(range(target_length), normalized_data[:, 3], label='gx', color='orange', alpha=0.7)
    axes[1, 1].plot(range(target_length), normalized_data[:, 4], label='gy', color='purple', alpha=0.7)
    axes[1, 1].plot(range(target_length), normalized_data[:, 5], label='gz', color='brown', alpha=0.7)
    axes[1, 1].set_title(f'归一化后陀螺仪数据 (长度: {target_length})')
    axes[1, 1].set_xlabel('时间索引')
    axes[1, 1].set_ylabel('角速度值')
    axes[1, 1].legend()
    axes[1, 1].grid(True, linestyle='--', alpha=0.3)
    
    # 单独特征对比 (ax为例)
    axes[2, 0].plot(range(original_length), original_data[:, 0], label='原始ax数据', color='red', alpha=0.7)
    axes[2, 0].set_title(f'原始ax数据 (长度: {original_length})')
    axes[2, 0].set_xlabel('时间索引')
    axes[2, 0].set_ylabel('ax值')
    axes[2, 0].legend()
    axes[2, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[2, 1].plot(range(target_length), normalized_data[:, 0], label=f'归一化ax数据', color='red', alpha=0.7)
    axes[2, 1].set_title(f'归一化后ax数据 (长度: {target_length})')
    axes[2, 1].set_xlabel('时间索引')
    axes[2, 1].set_ylabel('ax值')
    axes[2, 1].legend()
    axes[2, 1].grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    # 打印统计信息
    print("\n数据统计信息对比:")
    print(f"原始数据 - 平均值: ax={np.mean(original_data[:, 0]):.3f}, ay={np.mean(original_data[:, 1]):.3f}, az={np.mean(original_data[:, 2]):.3f}")
    print(f"归一化数据 - 平均值: ax={np.mean(normalized_data[:, 0]):.3f}, ay={np.mean(normalized_data[:, 1]):.3f}, az={np.mean(normalized_data[:, 2]):.3f}")
    print(f"原始数据 - 标准差: ax={np.std(original_data[:, 0]):.3f}, ay={np.std(original_data[:, 1]):.3f}, az={np.std(original_data[:, 2]):.3f}")
    print(f"归一化数据 - 标准差: ax={np.std(normalized_data[:, 0]):.3f}, ay={np.std(normalized_data[:, 1]):.3f}, az={np.std(normalized_data[:, 2]):.3f}")

if __name__ == "__main__":
    # 使用提供的数据文件路径
    file_path = 'test/none_001.csv'
    print("显示原始可视化:")
    visualize_gyro_data(file_path)
    
    print("\n" + "="*50)
    print("显示原始数据与归一化数据对比:")
    compare_original_and_normalized(file_path)
    
    # 可以取消下面的注释来测试其他文件
    # file_path = 'test/circular _001.csv'
    # print("\n" + "="*50)
    # print("显示 circular _001.csv 的对比:")
    # compare_original_and_normalized(file_path)
    #
    # file_path = 'test/lightning_001.csv'
    # print("\n" + "="*50)
    # print("显示 lightning_001.csv 的对比:")
    # compare_original_and_normalized(file_path)