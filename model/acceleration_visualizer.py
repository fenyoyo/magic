import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D


LSB_PER_G = 16384  # ±2g 量程时
G_TO_MS2 = 9.8     # 1g = 9.8 m/s²

def integrate_acceleration_to_position(data):
    """
    将加速度数据积分到位置数据
    数据格式：[time, sequence, x_acc, y_acc, z_acc]
    """
    # 加速度计参数
    LSB_PER_G = 16384  # ±2g 量程时
    G_TO_MS2 = 9.8     # 1g = 9.8 m/s²

    dt_ms = data[:, 0]  # 时间间隔，毫秒
    dt = dt_ms / 1000.0  # 转换为秒
    ax_lsb = data[:, 2]  # x轴加速度 (LSB)
    ay_lsb = data[:, 3]  # y轴加速度 (LSB)
    az_lsb = data[:, 4]  # z轴加速度 (LSB)

    # 转换LSB到m/s²
    ax_ms2 = (ax_lsb / LSB_PER_G) * G_TO_MS2
    ay_ms2 = (ay_lsb / LSB_PER_G) * G_TO_MS2
    az_ms2 = (az_lsb / LSB_PER_G) * G_TO_MS2

    # 初始化速度和位置数组
    vx = np.zeros(len(ax_lsb))
    vy = np.zeros(len(ay_lsb))
    vz = np.zeros(len(az_lsb))

    px = np.zeros(len(ax_lsb))
    py = np.zeros(len(ay_lsb))
    pz = np.zeros(len(az_lsb))

    # 积分计算速度（从加速度）
    # 使用梯形法则进行更精确的积分
    for i in range(1, len(ax_lsb)):
        # 计算平均加速度
        avg_ax = ax_ms2[i]
        avg_ay = ay_ms2[i]
        avg_az = az_ms2[i]

        # 使用平均加速度计算速度增量
        dvx = avg_ax * dt[i]
        dvy = avg_ay * dt[i]
        dvz = avg_az * dt[i]

        # 更新速度
        vx[i] = vx[i] + dvx
        vy[i] = vy[i] + dvy
        vz[i] = vz[i] + dvz

        # 使用平均速度计算位置增量
        avg_vx = vx[i]
        avg_vy = vy[i]
        avg_vz = vz[i]

        # 更新位置
        dx = avg_vx * dt[i]
        dy = avg_vy * dt[i]
        dz = avg_vz * dt[i]

        px[i] = px[i-1] + dx
        py[i] = py[i-1] + dy
        pz[i] = pz[i-1] + dz

    return px, py, pz, vx, vy, vz, ax_ms2, ay_ms2, az_ms2

def visualize_3d_trajectory(data):
    """可视化3D轨迹"""
    px, py, pz, vx, vy, vz, ax_ms2, ay_ms2, az_ms2 = integrate_acceleration_to_position(data)

    # 创建3D图形
    fig = plt.figure(figsize=(20, 15))

    # 绘制3D轨迹
    ax1 = fig.add_subplot(331, projection='3d')
    ax1.plot(px, py, pz, 'b-', linewidth=2)
    ax1.scatter(px[0], py[0], pz[0], color='green', s=100, label='Start', depthshade=False)
    ax1.scatter(px[-1], py[-1], pz[-1], color='red', s=100, label='End', depthshade=False)
    ax1.set_xlabel('X Position')
    ax1.set_ylabel('Y Position')
    ax1.set_zlabel('Z Position')
    ax1.set_title('3D Trajectory')
    ax1.legend()

    # 绘制XY平面的二维轨迹
    ax2 = fig.add_subplot(332)
    ax2.plot(px, py, 'b-', linewidth=2)
    ax2.scatter(px[0], py[0], color='green', s=100, label='Start')
    ax2.scatter(px[-1], py[-1], color='red', s=100, label='End')
    ax2.set_xlabel('X Position (m)')
    ax2.set_ylabel('Y Position (m)')
    ax2.set_title('XY Plane Projection')
    ax2.legend()
    ax2.grid(True)
    ax2.axis('equal')

    # 绘制XZ平面的二维轨迹
    ax3 = fig.add_subplot(333)
    ax3.plot(px, pz, 'b-', linewidth=2)
    ax3.scatter(px[0], pz[0], color='green', s=100, label='Start')
    ax3.scatter(px[-1], pz[-1], color='red', s=100, label='End')
    ax3.set_xlabel('X Position (m)')
    ax3.set_ylabel('Z Position (m)')
    ax3.set_title('XZ Plane Projection')
    ax3.legend()
    ax3.grid(True)
    ax3.axis('equal')

    # 绘制YZ平面的二维轨迹
    ax4 = fig.add_subplot(334)
    ax4.plot(py, pz, 'b-', linewidth=2)
    ax4.scatter(py[0], pz[0], color='green', s=100, label='Start')
    ax4.scatter(py[-1], pz[-1], color='red', s=100, label='End')
    ax4.set_xlabel('Y Position (m)')
    ax4.set_ylabel('Z Position (m)')
    ax4.set_title('YZ Plane Projection')
    ax4.legend()
    ax4.grid(True)
    ax4.axis('equal')

    # 绘制各轴的位置变化
    ax5 = fig.add_subplot(335)
    ax5.plot(px, label='X Position', color='red')
    ax5.plot(py, label='Y Position', color='green')
    ax5.plot(pz, label='Z Position', color='blue')
    ax5.set_xlabel('Time Step')
    ax5.set_ylabel('Position (m)')
    ax5.set_title('Position vs Time')
    ax5.legend()
    ax5.grid(True)

    # 绘制各轴的速度变化
    ax6 = fig.add_subplot(336)
    ax6.plot(vx, label='X Velocity', color='red')
    ax6.plot(vy, label='Y Velocity', color='green')
    ax6.plot(vz, label='Z Velocity', color='blue')
    ax6.set_xlabel('Time Step')
    ax6.set_ylabel('Velocity (m/s)')
    ax6.set_title('Velocity vs Time')
    ax6.legend()
    ax6.grid(True)

    # 绘制各轴的加速度变化
    ax7 = fig.add_subplot(337)
    ax7.plot(ax_ms2, label='X Accel', color='red')
    ax7.plot(ay_ms2, label='Y Accel', color='green')
    ax7.plot(az_ms2, label='Z Accel', color='blue')
    ax7.set_xlabel('Time Step')
    ax7.set_ylabel('Acceleration (m/s²)')
    ax7.set_title('Acceleration vs Time')
    ax7.legend()
    ax7.grid(True)

    plt.tight_layout()
    plt.show()

    # 打印一些统计数据
    print(f"总移动距离（直线距离）: {np.sqrt((px[-1]-px[0])**2 + (py[-1]-py[0])**2 + (pz[-1]-pz[0])**2):.6f} m")
    print(f"X方向最终位移: {px[-1] - px[0]:.6f} m")
    print(f"Y方向最终位移: {py[-1] - py[0]:.6f} m")
    print(f"Z方向最终位移: {pz[-1] - pz[0]:.6f} m")
    print(f"最终速度: X={vx[-1]:.6f}, Y={vy[-1]:.6f}, Z={vz[-1]:.6f} m/s")

    return px, py, pz, vx, vy, vz

def show(filename):
    df = pd.read_csv(filename)
    data = df[['dt', 'seq', 'x', 'y', 'z']].values
    visualize_3d_trajectory(data)

# 如果直接运行此脚本，则执行以下代码
if __name__ == "__main__":
    
    # 读取数据
    df = pd.read_csv('triangle/temp.csv')
    data = df[['dt', 'seq', 'x', 'y', 'z']].values

    print("原始加速度数据:")
    print(df.head())
    print("\n开始积分计算位置...")

    # 可视化
    px, py, pz, vx, vy, vz = visualize_3d_trajectory(data)

    # 显示详细结果
    print("\n详细位置数据:")
    for i in range(len(px)):
        print(f"Step {i}: Pos({px[i]:.6f}, {py[i]:.6f}, {pz[i]:.6f}), Vel({vx[i]:.6f}, {vy[i]:.6f}, {vz[i]:.6f})")


