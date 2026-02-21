import numpy as np
from time_normalization import time_normalize_sequence

def test_time_normalization():
    print("测试时间归一化功能...")
    
    # 创建一个模拟的传感器数据 (时间步长, 特征数)
    # 模拟一段时间内的加速度和陀螺仪数据
    original_length = 150
    target_length = 100
    num_features = 6  # ax, ay, az, gx, gy, gz
    
    # 创建一些模拟数据 - 正弦波形，代表某种周期性运动
    t = np.linspace(0, 4*np.pi, original_length)
    data = np.zeros((original_length, num_features))
    
    # 前3列是加速度数据，后3列是陀螺仪数据
    data[:, 0] = np.sin(t)           # ax
    data[:, 1] = np.cos(t)           # ay
    data[:, 2] = np.sin(t * 0.5)     # az
    data[:, 3] = np.cos(t * 2)       # gx
    data[:, 4] = np.sin(t * 1.5)     # gy
    data[:, 5] = np.cos(t * 0.8)     # gz
    
    print(f"原始数据形状: {data.shape}")
    
    # 测试时间归一化
    normalized_data = time_normalize_sequence(data, target_length)
    print(f"归一化后数据形状: {normalized_data.shape}")
    
    # 验证数据没有异常值
    assert normalized_data.shape == (target_length, num_features), "形状不匹配"
    assert not np.any(np.isnan(normalized_data)), "存在NaN值"
    assert not np.any(np.isinf(normalized_data)), "存在无穷大值"
    
    print("时间归一化功能测试通过!")
    
    # 测试不同长度的输入
    short_data = data[:50, :]  # 较短的序列
    normalized_short = time_normalize_sequence(short_data, target_length)
    print(f"扩展前形状: {short_data.shape}, 扩展后形状: {normalized_short.shape}")
    
    long_data = data[:200, :]   # 较长的序列
    normalized_long = time_normalize_sequence(long_data, target_length)
    print(f"压缩前形状: {long_data.shape}, 压缩后形状: {normalized_long.shape}")
    
    print("所有测试通过!")

if __name__ == "__main__":
    test_time_normalization()