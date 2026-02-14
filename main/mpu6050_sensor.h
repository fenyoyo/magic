#ifndef _MPU6050_SENSOR_H_
#define _MPU6050_SENSOR_H_

#include "mpu6050.h"

/** 一秒内陀螺仪采样最大点数（按 50Hz 约 50 个点） */
#define MPU6050_GYRO_SAMPLES_MAX 50

/**
 * MPU6050 传感器数据（加速度 g，角速度 °/s，温度 °C）
 */
struct MPU6050Data
{
    float accel_x, accel_y, accel_z;  // 加速度 (g)
    float gyro_x, gyro_y, gyro_z;     // 陀螺仪 (°/s)
    float temperature;                 // 温度 (°C)
};

/**
 * MPU6050 传感器封装：初始化与数据获取
 */
class MPU6050Sensor
{
public:
    static MPU6050Sensor &getInstance();

    MPU6050Sensor(const MPU6050Sensor &) = delete;
    MPU6050Sensor &operator=(const MPU6050Sensor &) = delete;

    /** 初始化并启动 MPU6050，成功返回 true */
    bool init();

    /** 是否已成功初始化 */
    bool isReady() const { return m_ready; }

    /** 读取加速度、陀螺仪、温度到 data，成功返回 true */
    bool getData(MPU6050Data &data);

    /** 仅读取陀螺仪角速度 (°/s)，成功返回 true */
    bool getGyro(float &gx, float &gy, float &gz);


private:
    MPU6050Sensor();
    ~MPU6050Sensor();

    bool m_ready;
    mpu6050_dev_t m_dev;
};

#endif // _MPU6050_SENSOR_H_
