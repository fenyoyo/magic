#pragma once
#include "kalman_filter.h"
#include "mpu6050_sensor.h"
#include <array>

namespace gamic {

/// 专为MPU6050传感器数据设计的卡尔曼滤波器
/// 用于过滤加速度计和陀螺仪数据
class MPU6050KalmanFilter {
public:
    /// 构造函数，创建6维状态向量的卡尔曼滤波器 (gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z)
    MPU6050KalmanFilter() : kalman_filter_() {
        // 设置默认的噪声参数
        std::array<float, 6> process_noise = {0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.02f};
        std::array<float, 6> measurement_noise = {0.1f, 0.1f, 0.1f, 0.2f, 0.2f, 0.2f};
        
        kalman_filter_.set_process_noise(process_noise);
        kalman_filter_.set_measurement_noise(measurement_noise);
    }

    /// 使用原始MPU6050数据更新滤波器并返回滤波后的数据
    /// @param raw_data 原始MPU6050传感器数据
    /// @param dt 时间间隔（秒）
    /// @return 滤波后的MPU6050数据
    MPU6050Data filterData(const MPU6050Data& raw_data, float dt = 0.02f) {
        // 将原始数据转换为数组格式
        std::array<float, 6> measurements = {
            raw_data.gyro_x,
            raw_data.gyro_y,
            raw_data.gyro_z,
            raw_data.accel_x,
            raw_data.accel_y,
            raw_data.accel_z
        };

        // 预测步骤 - 使用零控制输入
        std::array<float, 6> control_input = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        kalman_filter_.predict(control_input, dt);

        // 更新步骤 - 使用测量值
        kalman_filter_.update(measurements);

        // 获取滤波后的状态
        const auto& filtered_state = kalman_filter_.get_state();

        // 构建结果数据
        MPU6050Data filtered_data;
        filtered_data.gyro_x = filtered_state[0];
        filtered_data.gyro_y = filtered_state[1];
        filtered_data.gyro_z = filtered_state[2];
        filtered_data.accel_x = filtered_state[3];
        filtered_data.accel_y = filtered_state[4];
        filtered_data.accel_z = filtered_state[5];

        return filtered_data;
    }

    /// 重置滤波器
    void reset() {
        kalman_filter_.reset();
    }

    /// 设置过程噪声
    void setProcessNoise(float gyro_noise, float accel_noise) {
        std::array<float, 6> process_noise = {
            gyro_noise, gyro_noise, gyro_noise,
            accel_noise, accel_noise, accel_noise
        };
        kalman_filter_.set_process_noise(process_noise);
    }

    /// 设置测量噪声
    void setMeasurementNoise(float gyro_noise, float accel_noise) {
        std::array<float, 6> measurement_noise = {
            gyro_noise, gyro_noise, gyro_noise,
            accel_noise, accel_noise, accel_noise
        };
        kalman_filter_.set_measurement_noise(measurement_noise);
    }

private:
    KalmanFilter<6> kalman_filter_;  // 6维卡尔曼滤波器 (3轴陀螺仪 + 3轴加速度计)
};

} // namespace gamic