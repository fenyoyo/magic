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
 * 一秒内采集的陀螺仪数据（°/s）
 */
struct MPU6050GyroSnapshot
{
    float gyro_x[MPU6050_GYRO_SAMPLES_MAX];
    float gyro_y[MPU6050_GYRO_SAMPLES_MAX];
    float gyro_z[MPU6050_GYRO_SAMPLES_MAX];
    int count;  // 实际采样数
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

    /**
     * 在一秒内按固定间隔采集陀螺仪数据，写入 out。
     * 采样间隔约 20ms（50Hz），最多 MPU6050_GYRO_SAMPLES_MAX 个点。
     * 成功返回 true，out.count 为实际采样数。
     */
    bool readGyroForOneSecond(MPU6050GyroSnapshot &out);

private:
    MPU6050Sensor();
    ~MPU6050Sensor();

    bool m_ready;
    mpu6050_dev_t m_dev;
};

#endif // _MPU6050_SENSOR_H_
