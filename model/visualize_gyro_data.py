import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import seaborn as sns
import argparse
import os


def load_gyro_data(csv_file):
    """
    从CSV文件加载陀螺仪数据
    
    Args:
        csv_file (str): CSV文件路径
        
    Returns:
        pandas.DataFrame: 包含陀螺仪数据的DataFrame
    """
    try:
        df = pd.read_csv(csv_file)
        return df
    except FileNotFoundError:
        print(f"错误: 找不到文件 {csv_file}")
        return None
    except Exception as e:
        print(f"读取文件时出错: {str(e)}")
        return None


def plot_3d_trajectory(df, x_col='x', y_col='y', z_col='z'):
    """
    绘制3D运动轨迹图
    
    Args:
        df (pandas.DataFrame): 包含陀螺仪数据的DataFrame
        x_col (str): X轴列名
        y_col (str): Y轴列名
        z_col (str): Z轴列名
    """
    fig = plt.figure(figsize=(12, 9))
    ax = fig.add_subplot(111, projection='3d')
    
    # 检查指定的列是否存在
    required_cols = [x_col, y_col, z_col]
    missing_cols = [col for col in required_cols if col not in df.columns]
    
    if missing_cols:
        print(f"警告: 缺少以下列: {missing_cols}")
        # 尝试使用默认的陀螺仪列名
        possible_cols = ['gx', 'gy', 'gz', 'x', 'y', 'z', 'rotation_x', 'rotation_y', 'rotation_z']
        available_cols = [col for col in possible_cols if col in df.columns]
        
        if len(available_cols) >= 3:
            x_col, y_col, z_col = available_cols[:3]
            print(f"使用可用列: {x_col}, {y_col}, {z_col}")
        else:
            print("无法找到足够的陀螺仪数据列")
            return
    
    ax.plot(df[x_col], df[y_col], df[z_col], label='Gyroscope Trajectory', alpha=0.7)
    ax.scatter(df[x_col][0], df[y_col][0], df[z_col][0], color='green', s=100, label='Start', zorder=5)
    ax.scatter(df[x_col].iloc[-1], df[y_col].iloc[-1], df[z_col].iloc[-1], color='red', s=100, label='End', zorder=5)
    
    ax.set_xlabel(x_col)
    ax.set_ylabel(y_col)
    ax.set_zlabel(z_col)
    ax.set_title('3D Gyroscope Data Trajectory')
    ax.legend()
    
    plt.tight_layout()
    return fig


def plot_2d_projections(df, x_col='x', y_col='y', z_col='z'):
    """
    绘制2D投影图（XY、XZ、YZ平面）
    
    Args:
        df (pandas.DataFrame): 包含陀螺仪数据的DataFrame
        x_col (str): X轴列名
        y_col (str): Y轴列名
        z_col (str): Z轴列名
    """
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    
    # 检查指定的列是否存在
    required_cols = [x_col, y_col, z_col]
    missing_cols = [col for col in required_cols if col not in df.columns]
    
    if missing_cols:
        print(f"警告: 缺少以下列: {missing_cols}")
        # 尝试使用默认的陀螺仪列名
        possible_cols = ['gx', 'gy', 'gz', 'x', 'y', 'z', 'rotation_x', 'rotation_y', 'rotation_z']
        available_cols = [col for col in possible_cols if col in df.columns]
        
        if len(available_cols) >= 3:
            x_col, y_col, z_col = available_cols[:3]
            print(f"使用可用列: {x_col}, {y_col}, {z_col}")
        else:
            print("无法找到足够的陀螺仪数据列")
            return
    
    # XY平面
    axes[0, 0].plot(df[x_col], df[y_col])
    axes[0, 0].scatter(df[x_col][0], df[y_col][0], color='green', s=50, label='Start', zorder=5)
    axes[0, 0].scatter(df[x_col].iloc[-1], df[y_col].iloc[-1], color='red', s=50, label='End', zorder=5)
    axes[0, 0].set_xlabel(x_col)
    axes[0, 0].set_ylabel(y_col)
    axes[0, 0].set_title(f'{x_col} vs {y_col}')
    axes[0, 0].grid(True, linestyle='--', alpha=0.6)
    axes[0, 0].legend()
    
    # XZ平面
    axes[0, 1].plot(df[x_col], df[z_col])
    axes[0, 1].scatter(df[x_col][0], df[z_col][0], color='green', s=50, label='Start', zorder=5)
    axes[0, 1].scatter(df[x_col].iloc[-1], df[z_col].iloc[-1], color='red', s=50, label='End', zorder=5)
    axes[0, 1].set_xlabel(x_col)
    axes[0, 1].set_ylabel(z_col)
    axes[0, 1].set_title(f'{x_col} vs {z_col}')
    axes[0, 1].grid(True, linestyle='--', alpha=0.6)
    axes[0, 1].legend()
    
    # YZ平面
    axes[1, 0].plot(df[y_col], df[z_col])
    axes[1, 0].scatter(df[y_col][0], df[z_col][0], color='green', s=50, label='Start', zorder=5)
    axes[1, 0].scatter(df[y_col].iloc[-1], df[z_col].iloc[-1], color='red', s=50, label='End', zorder=5)
    axes[1, 0].set_xlabel(y_col)
    axes[1, 0].set_ylabel(z_col)
    axes[1, 0].set_title(f'{y_col} vs {z_col}')
    axes[1, 0].grid(True, linestyle='--', alpha=0.6)
    axes[1, 0].legend()
    
    # 时间序列图
    time_col = 'time' if 'time' in df.columns else df.index
    axes[1, 1].plot(time_col, df[x_col], label=f'{x_col}', linewidth=2)
    axes[1, 1].plot(time_col, df[y_col], label=f'{y_col}', linewidth=2)
    axes[1, 1].plot(time_col, df[z_col], label=f'{z_col}', linewidth=2)
    axes[1, 1].set_xlabel('Time')
    axes[1, 1].set_ylabel('Angular Velocity')
    axes[1, 1].set_title('Angular Velocity Over Time')
    axes[1, 1].legend()
    axes[1, 1].grid(True, linestyle='--', alpha=0.6)
    
    plt.tight_layout()
    return fig


def plot_time_series(df, x_col='x', y_col='y', z_col='z'):
    """
    绘制时间序列图
    
    Args:
        df (pandas.DataFrame): 包含陀螺仪数据的DataFrame
        x_col (str): X轴列名
        y_col (str): Y轴列名
        z_col (str): Z轴列名
    """
    fig, ax = plt.subplots(figsize=(15, 8))
    
    # 检查指定的列是否存在
    required_cols = [x_col, y_col, z_col]
    missing_cols = [col for col in required_cols if col not in df.columns]
    
    if missing_cols:
        print(f"警告: 缺少以下列: {missing_cols}")
        # 尝试使用默认的陀螺仪列名
        possible_cols = ['gx', 'gy', 'gz', 'x', 'y', 'z', 'rotation_x', 'rotation_y', 'rotation_z']
        available_cols = [col for col in possible_cols if col in df.columns]
        
        if len(available_cols) >= 3:
            x_col, y_col, z_col = available_cols[:3]
            print(f"使用可用列: {x_col}, {y_col}, {z_col}")
        else:
            print("无法找到足够的陀螺仪数据列")
            return
    
    time_col = 'time' if 'time' in df.columns else df.index
    
    ax.plot(time_col, df[x_col], label=f'{x_col} (X-axis)', linewidth=2)
    ax.plot(time_col, df[y_col], label=f'{y_col} (Y-axis)', linewidth=2)
    ax.plot(time_col, df[z_col], label=f'{z_col} (Z-axis)', linewidth=2)
    
    ax.set_xlabel('Time')
    ax.set_ylabel('Angular Velocity (rad/s)')
    ax.set_title('Gyroscope Angular Velocity Over Time')
    ax.legend()
    ax.grid(True, linestyle='--', alpha=0.6)
    
    plt.tight_layout()
    return fig


def visualize_gyro_data(csv_file, output_dir=None):
    """
    主函数：加载CSV文件并生成陀螺仪数据图表
    
    Args:
        csv_file (str): 输入的CSV文件路径
        output_dir (str): 输出目录路径，如果为None则在当前目录下保存
    """
    # 加载数据
    df = load_gyro_data(csv_file)
    if df is None:
        return

    print(f"成功加载数据，共 {len(df)} 行")
    print(f"数据列: {list(df.columns)}")
    
    # 设置绘图样式
    plt.style.use('seaborn-v0_8')
    sns.set_palette("husl")
    
    # 创建输出目录
    if output_dir is None:
        output_dir = os.path.dirname(csv_file) if os.path.isfile(csv_file) else "."
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 确定列名
    possible_cols = ['gx', 'gy', 'gz', 'ax', 'ay', 'az']
    gyro_cols = [col for col in possible_cols if col in df.columns]
    
    if len(gyro_cols) < 3:
        print(f"警告: 找到的陀螺仪列少于3个: {gyro_cols}")
        # 如果没有足够的陀螺仪列，则尝试使用所有数值列
        numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()
        if len(numeric_cols) >= 3:
            gyro_cols = numeric_cols[:3]
        else:
            print("错误: 没有足够的数值列用于绘图")
            return
    
    gx_col, gy_col, gz_col,ax_col,ay_col,az_col = gyro_cols
    
    print(f"使用列进行可视化: {gx_col}, {gy_col}, {gz_col}, {ax_col}, {ay_col}, {az_col}")
    
    # 生成3D轨迹图
    # fig_3d = plot_3d_trajectory(df, gx_col, gy_col, gz_col)
    # if fig_3d:
    #     output_path = os.path.join(output_dir, 'gyro_3d_trajectory.png')
    #     fig_3d.savefig(output_path, dpi=300, bbox_inches='tight')
    #     print(f"3D轨迹图已保存至: {output_path}")
    
    # 生成2D投影图
    # fig_2d = plot_2d_projections(df, gx_col, gy_col, gz_col)
    # if fig_2d:
    #     output_path = os.path.join(output_dir, 'gyro_2d_projections.png')
    #     fig_2d.savefig(output_path, dpi=300, bbox_inches='tight')
    #     print(f"2D投影图已保存至: {output_path}")
    
    # 生成时间序列图
    # fig_ts = plot_time_series(df, gx_col, gy_col, gz_col)
    # if fig_ts:
    #     output_path = os.path.join(output_dir, 'gyro_time_series.png')
    #     fig_ts.savefig(output_path, dpi=300, bbox_inches='tight')
    #     print(f"时间序列图已保存至: {output_path}")
    
    plt.show()


def main():
    """
    主函数，处理命令行参数并执行可视化
    """



    # file = './data/circle_001.csv'
    file = './triangle/gyro_data_20260220_015154.csv'

    visualize_gyro_data(file, './output')


if __name__ == "__main__":
    # 示例用法:
    # python visualize_gyro_data.py gyro_data.csv
    # python visualize_gyro_data.py gyro_data.csv -o ./output
    
    main()
    # pass