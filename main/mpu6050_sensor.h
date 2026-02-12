#ifndef MPU6050_SENSOR_H
#define MPU6050_SENSOR_H

#include <functional>
#include "driver/i2c.h"
#include "mpu6050.h"

#ifdef CONFIG_EXAMPLE_I2C_ADDRESS_LOW
#define ADDR MPU6050_I2C_ADDRESS_LOW
#else
#define ADDR MPU6050_I2C_ADDRESS_HIGH
#endif

class MPU6050Sensor
{
public:
    // 数据回调类型定义
    using DataCallback = std::function<void(const mpu6050_acceleration_t &accel,
                                            const mpu6050_rotation_t &gyro,
                                            float temperature)>;

    // 单例模式
    static MPU6050Sensor &getInstance();

    // 禁用拷贝构造和赋值操作
    MPU6050Sensor(const MPU6050Sensor &) = delete;
    MPU6050Sensor &operator=(const MPU6050Sensor &) = delete;

    // 初始化传感器
    bool begin(uint8_t addr = ADDR, int sda_pin = CONFIG_EXAMPLE_SDA_GPIO,
               int scl_pin = CONFIG_EXAMPLE_SCL_GPIO);

    // 校准陀螺仪
    void calibrateGyro(int samples = 2000);

    // 读取数据
    bool readData(mpu6050_acceleration_t &accel,
                  mpu6050_rotation_t &gyro,
                  float &temperature);

    // 获取校准后的陀螺仪数据
    mpu6050_rotation_t getCalibratedGyro();

    // 设置数据回调
    void setDataCallback(DataCallback callback) { m_data_callback = callback; }

    // 启动数据采集任务
    void startSampling(int interval_ms = 100);

    // 停止数据采集任务
    void stopSampling();

    // 状态查询
    bool isInitialized() const { return m_initialized; }
    bool isSampling() const { return m_sampling; }

    // 获取设备信息
    const char *getDeviceName() const { return "MPU6050"; }

    // 获取偏移量
    float getGyroXOffset() const { return m_gyro_offset.x; }
    float getGyroYOffset() const { return m_gyro_offset.y; }
    float getGyroZOffset() const { return m_gyro_offset.z; }

private:
    MPU6050Sensor();
    ~MPU6050Sensor();

    // 静态任务函数
    static void samplingTask(void *pvParameters);

    // 内部初始化
    bool initDevice();
    bool checkDevicePresence();

private:
    static const char *TAG;
    static MPU6050Sensor *s_instance;

    // MPU6050设备结构体
    mpu6050_dev_t m_dev;

    // 状态标志
    bool m_initialized;
    bool m_sampling;
    int m_sampling_interval_ms;

    // 校准数据
    mpu6050_rotation_t m_gyro_offset;
    bool m_calibrated;

    // 任务句柄
    TaskHandle_t m_task_handle;

    // 数据回调
    DataCallback m_data_callback;

    // 引脚配置
    int m_sda_pin;
    int m_scl_pin;
    uint8_t m_device_addr;
};

#endif // MPU6050_SENSOR_H