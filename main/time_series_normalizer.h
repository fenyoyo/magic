#ifndef TIME_SERIES_NORMALIZER_H
#define TIME_SERIES_NORMALIZER_H

#include <vector>
#include <cstddef>

/**
 * @brief 使用线性插值对时间序列进行归一化
 * 
 * @param sequence 输入的时间序列，形状为 (sequence_length, features)
 * @param target_length 目标长度
 * @return 归一化后的时间序列，形状为 (target_length, features)
 */
std::vector<std::vector<float>> time_normalize_sequence(
    const std::vector<std::vector<float>>& sequence, 
    size_t target_length);

/**
 * @brief 简化的MPU6050数据归一化函数，适用于固定6维特征向量
 * 
 * @param sequence 输入的时间序列，形状为 (sequence_length, 6) - [acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z]
 * @param target_length 目标长度
 * @return 归一化后的时间序列，形状为 (target_length, 6)
 */
std::vector<std::vector<float>> normalize_mpu6050_data(
    const std::vector<std::vector<float>>& sequence, 
    size_t target_length);

#endif // TIME_SERIES_NORMALIZER_H