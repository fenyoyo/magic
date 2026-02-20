#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
处理陀螺仪数据并绘制3D轨迹图
根据dt（时间间隔）、x,y,z（加速度）计算位置变化
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import seaborn as sns
from scipy import integrate


def load_and_process_gyro_data(csv_file):
    """
    从CSV文件加载陀螺仪数据并进行处理
    
    Args:
        csv_file (str): CSV文件路径
        
    Returns:
        pandas.DataFrame: 处理后的数据
    """
    try:
        df = pd.read_csv(csv_file)
        print(f"成功加载数据，共 {len(df)} 行")
        print(f"数据列: {list(df.columns)}")
        
        # 确保数据按seq排序
        df = df.sort_values('seq').reset_index(drop=True)
        
        # 计算真实时间戳（累积dt）
        df['timestamp'] = df['dt'].cumsum()
        
        # 将dt从毫秒转换为秒
        df['dt_seconds'] = df['dt'] / 1000.0
        
        return df
    except FileNotFoundError:
        print(f"错误: 找不到文件 {csv_file}")
        return None
    except Exception as e:
        print(f"读取文件时出错: {str(e)}")
        return None


def calculate_position_from_acceleration(df):
    """
    从加速度数据计算位置
    
    Args:
        df (pandas.DataFrame): 包含加速度数据的DataFrame
        
    Returns:
        pandas.DataFrame: 包含位置信息的DataFrame
    """
    # 提取加速度数据
    ax = df['x'].values
    ay = df['y'].values
    az = df['z'].values
    dt = df['dt_seconds'].values
    
    # 初始化速度和位置数组
    vx = np.zeros(len(ax))  # x方向速度
    vy = np.zeros(len(ay))  # y方向速度
    vz = np.zeros(len(az))  # z方向速度
    
    px = np.zeros(len(ax))  # x方向位置
    py = np.zeros(len(ay))  # y方向位置
    pz = np.zeros(len(az))  # z方向位置
    
    # 数值积分：速度 = 加速度 × 时间
    for i in range(1, len(dt)):
        # 更新速度 (v = v0 + a*dt)
        vx[i] = vx[i-1] + ax[i] * dt[i]
        vy[i] = vy[i-1] + ay[i] * dt[i]
        vz[i] = vz[i-1] + az[i] * dt[i]
        
        # 更新位置 (p = p0 + v*dt)
        px[i] = px[i-1] + vx[i] * dt[i]
        py[i] = py[i-1] + vy[i] * dt[i]
        pz[i] = pz[i-1] + vz[i] * dt[i]
    
    # 将结果添加到DataFrame
    df['vx'] = vx
    df['vy'] = vy
    df['vz'] = vz
    df['px'] = px
    df['py'] = py
    df['pz'] = pz
    
    return df


def plot_3d_trajectory(df):
    """
    绘制3D运动轨迹图
    
    Args:
        df (pandas.DataFrame): 包含位置数据的DataFrame
    """
    fig = plt.figure(figsize=(14, 10))
    
    # 3D轨迹图
    ax1 = fig.add_subplot(221, projection='3d')
    ax1.plot(df['px'], df['py'], df['pz'], label='Position Trajectory', alpha=0.7, linewidth=2)
    ax1.scatter(df['px'][0], df['py'][0], df['pz'][0], color='green', s=100, label='Start', zorder=5)
    ax1.scatter(df['px'].iloc[-1], df['py'].iloc[-1], df['pz'].iloc[-1], color='red', s=100, label='End', zorder=5)
    
    ax1.set_xlabel('X Position')
    ax1.set_ylabel('Y Position')
    ax1.set_zlabel('Z Position')
    ax1.set_title('3D Position Trajectory')
    ax1.legend()
    
    # XY平面投影
    ax2 = fig.add_subplot(222)
    ax2.plot(df['px'], df['py'], label='XY Projection', alpha=0.7, linewidth=2)
    ax2.scatter(df['px'][0], df['py'][0], color='green', s=100, label='Start', zorder=5)
    ax2.scatter(df['px'].iloc[-1], df['py'].iloc[-1], color='red', s=100, label='End', zorder=5)
    ax2.set_xlabel('X Position')
    ax2.set_ylabel('Y Position')
    ax2.set_title('XY Projection')
    ax2.grid(True, linestyle='--', alpha=0.6)
    ax2.legend()
    
    # XZ平面投影
    ax3 = fig.add_subplot(223)
    ax3.plot(df['px'], df['pz'], label='XZ Projection', alpha=0.7, linewidth=2)
    ax3.scatter(df['px'][0], df['pz'][0], color='green', s=100, label='Start', zorder=5)
    ax3.scatter(df['px'].iloc[-1], df['pz'].iloc[-1], color='red', s=100, label='End', zorder=5)
    ax3.set_xlabel('X Position')
    ax3.set_ylabel('Z Position')
    ax3.set_title('XZ Projection')
    ax3.grid(True, linestyle='--', alpha=0.6)
    ax3.legend()
    
    # YZ平面投影
    ax4 = fig.add_subplot(224)
    ax4.plot(df['py'], df['pz'], label='YZ Projection', alpha=0.7, linewidth=2)
    ax4.scatter(df['py'][0], df['pz'][0], color='green', s=100, label='Start', zorder=5)
    ax4.scatter(df['py'].iloc[-1], df['pz'].iloc[-1], color='red', s=100, label='End', zorder=5)
    ax4.set_xlabel('Y Position')
    ax4.set_ylabel('Z Position')
    ax4.set_title('YZ Projection')
    ax4.grid(True, linestyle='--', alpha=0.6)
    ax4.legend()
    
    plt.tight_layout()
    return fig


def plot_velocity_and_acceleration(df):
    """
    绘制速度和加速度的时间序列图
    
    Args:
        df (pandas.DataFrame): 包含数据的DataFrame
    """
    fig, axes = plt.subplots(3, 2, figsize=(15, 12))
    
    # 加速度时间序列
    axes[0, 0].plot(df['timestamp'], df['x'], label='X Acceleration', linewidth=2)
    axes[0, 0].plot(df['timestamp'], df['y'], label='Y Acceleration', linewidth=2)
    axes[0, 0].plot(df['timestamp'], df['z'], label='Z Acceleration', linewidth=2)
    axes[0, 0].set_xlabel('Time (ms)')
    axes[0, 0].set_ylabel('Acceleration (LSB)')
    axes[0, 0].set_title('Acceleration Over Time')
    axes[0, 0].legend()
    axes[0, 0].grid(True, linestyle='--', alpha=0.6)
    
    # 速度时间序列
    axes[1, 0].plot(df['timestamp'], df['vx'], label='X Velocity', linewidth=2)
    axes[1, 0].plot(df['timestamp'], df['vy'], label='Y Velocity', linewidth=2)
    axes[1, 0].plot(df['timestamp'], df['vz'], label='Z Velocity', linewidth=2)
    axes[1, 0].set_xlabel('Time (ms)')
    axes[1, 0].set_ylabel('Velocity')
    axes[1, 0].set_title('Velocity Over Time')
    axes[1, 0].legend()
    axes[1, 0].grid(True, linestyle='--', alpha=0.6)
    
    # 位置时间序列
    axes[2, 0].plot(df['timestamp'], df['px'], label='X Position', linewidth=2)
    axes[2, 0].plot(df['timestamp'], df['py'], label='Y Position', linewidth=2)
    axes[2, 0].plot(df['timestamp'], df['pz'], label='Z Position', linewidth=2)
    axes[2, 0].set_xlabel('Time (ms)')
    axes[2, 0].set_ylabel('Position')
    axes[2, 0].set_title('Position Over Time')
    axes[2, 0].legend()
    axes[2, 0].grid(True, linestyle='--', alpha=0.6)
    
    # 3D速度向量轨迹
    axes[0, 1].plot(df['vx'], df['vy'], label='XY Velocity', alpha=0.7, linewidth=2)
    axes[0, 1].scatter(df['vx'][0], df['vy'][0], color='green', s=100, label='Start', zorder=5)
    axes[0, 1].scatter(df['vx'].iloc[-1], df['vy'].iloc[-1], color='red', s=100, label='End', zorder=5)
    axes[0, 1].set_xlabel('X Velocity')
    axes[0, 1].set_ylabel('Y Velocity')
    axes[0, 1].set_title('XY Velocity Plane')
    axes[0, 1].legend()
    axes[0, 1].grid(True, linestyle='--', alpha=0.6)
    
    # 3D位置向量轨迹
    axes[1, 1].plot(df['px'], df['py'], label='XY Position', alpha=0.7, linewidth=2)
    axes[1, 1].scatter(df['px'][0], df['py'][0], color='green', s=100, label='Start', zorder=5)
    axes[1, 1].scatter(df['px'].iloc[-1], df['py'].iloc[-1], color='red', s=100, label='End', zorder=5)
    axes[1, 1].set_xlabel('X Position')
    axes[1, 1].set_ylabel('Y Position')
    axes[1, 1].set_title('XY Position Plane')
    axes[1, 1].legend()
    axes[1, 1].grid(True, linestyle='--', alpha=0.6)
    
    # 总移动距离
    distance_xyz = np.sqrt((df['px'] - df['px'][0])**2 + (df['py'] - df['py'][0])**2 + (df['pz'] - df['pz'][0])**2)
    axes[2, 1].plot(df['timestamp'], distance_xyz, label='Distance from Start', linewidth=2, color='purple')
    axes[2, 1].set_xlabel('Time (ms)')
    axes[2, 1].set_ylabel('Distance from Start')
    axes[2, 1].set_title('Distance from Starting Point')
    axes[2, 1].legend()
    axes[2, 1].grid(True, linestyle='--', alpha=0.6)
    
    plt.tight_layout()
    return fig


def main():
    """
    主函数
    """
    # 创建一个示例数据文件，包含您提供的数据
    sample_data = """dt,seq,x,y,z
10.0,0,-21,26,102
20.0,1,-8,21,82
20.0,2,16,2,44
20.0,3,13,3,35
20.0,4,13,1,29
20.0,5,10,6,23
20.0,6,3,6,24
20.0,7,4,-1,34
20.0,8,-4,-10,36
20.0,9,-1,-17,35
20.0,10,4,-13,32
20.0,11,7,-14,25
20.0,12,7,-4,32
20.0,13,11,-2,26
20.0,14,5,-2,28
20.0,15,7,-10,28
20.0,16,3,-12,29
20.0,17,3,-9,28
20.0,18,-1,-3,25
20.0,19,0,-5,22
20.0,20,4,-4,23
20.0,21,4,-1,23
20.0,22,0,-1,23
20.0,23,1,0,23"""
    
    # 写入临时文件
    with open('temp_gyro_data.csv', 'w') as f:
        f.write(sample_data)
    
    # 加载数据
    df = load_and_process_gyro_data('temp_gyro_data.csv')
    if df is None:
        return
    
    # 计算位置
    df = calculate_position_from_acceleration(df)
    
    print("\n数据统计:")
    print(f"总时间: {df['timestamp'].iloc[-1]:.2f} ms")
    print(f"最终位置: X={df['px'].iloc[-1]:.2f}, Y={df['py'].iloc[-1]:.2f}, Z={df['pz'].iloc[-1]:.2f}")
    print(f"最大速度: VX={df['vx'].abs().max():.2f}, VY={df['vy'].abs().max():.2f}, VZ={df['vz'].abs().max():.2f}")
    
    # 绘制3D轨迹图
    fig1 = plot_3d_trajectory(df)
    fig1.suptitle('Gyroscope Data: 3D Position Trajectory', fontsize=16)
    
    # 绘制速度和加速度图
    fig2 = plot_velocity_and_acceleration(df)
    fig2.suptitle('Gyroscope Data: Acceleration, Velocity, and Position Analysis', fontsize=16)
    
    plt.show()


if __name__ == "__main__":
    main()