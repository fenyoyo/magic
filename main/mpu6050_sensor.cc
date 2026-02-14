#include "mpu6050_sensor.h"
#include <cstring>
#include <esp_log.h>
#include "i2cdev.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MPU6050"

#ifdef CONFIG_EXAMPLE_I2C_ADDRESS_LOW
#define MPU6050_ADDR MPU6050_I2C_ADDRESS_LOW
#else
#define MPU6050_ADDR MPU6050_I2C_ADDRESS_HIGH
#endif

#define I2C_PORT I2C_NUM_0

MPU6050Sensor::MPU6050Sensor() : m_ready(false)
{
    memset(&m_dev, 0, sizeof(m_dev));
}

MPU6050Sensor::~MPU6050Sensor()
{
    if (m_ready)
    {
        mpu6050_free_desc(&m_dev);
        m_ready = false;
    }
}

MPU6050Sensor &MPU6050Sensor::getInstance()
{
    static MPU6050Sensor instance;
    return instance;
}

bool MPU6050Sensor::init()
{
    if (m_ready)
        return true;

    static bool i2c_subsystem_inited = false;
    if (!i2c_subsystem_inited)
    {
        esp_err_t err = i2cdev_init();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "i2cdev_init failed: %s", esp_err_to_name(err));
            return false;
        }
        i2c_subsystem_inited = true;
    }

    gpio_num_t sda = (gpio_num_t)CONFIG_EXAMPLE_SDA_GPIO;
    gpio_num_t scl = (gpio_num_t)CONFIG_EXAMPLE_SCL_GPIO;

    esp_err_t err = mpu6050_init_desc(&m_dev, MPU6050_ADDR, I2C_PORT, sda, scl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mpu6050_init_desc failed: %s", esp_err_to_name(err));
        return false;
    }

    err = mpu6050_init(&m_dev);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mpu6050_init failed: %s", esp_err_to_name(err));
        mpu6050_free_desc(&m_dev);
        return false;
    }

    m_ready = true;
    ESP_LOGI(TAG, "MPU6050 init OK (SDA=%d, SCL=%d, addr=0x%02x)", sda, scl, (unsigned)MPU6050_ADDR);
    return true;
}

bool MPU6050Sensor::getData(MPU6050Data &data)
{
    if (!m_ready)
        return false;

    mpu6050_acceleration_t accel;
    mpu6050_rotation_t gyro;

    if (mpu6050_get_acceleration(&m_dev, &accel) != ESP_OK)
        return false;
    if (mpu6050_get_rotation(&m_dev, &gyro) != ESP_OK)
        return false;

    data.accel_x = accel.x;
    data.accel_y = accel.y;
    data.accel_z = accel.z;
    data.gyro_x = gyro.x;
    data.gyro_y = gyro.y;
    data.gyro_z = gyro.z;

    data.temperature = 0.0f;
    if (mpu6050_get_temperature(&m_dev, &data.temperature) != ESP_OK)
        data.temperature = 0.0f;

    return true;
}

bool MPU6050Sensor::getGyro(float &gx, float &gy, float &gz)
{
    if (!m_ready)
        return false;
    mpu6050_rotation_t gyro;
    if (mpu6050_get_rotation(&m_dev, &gyro) != ESP_OK)
        return false;
    gx = gyro.x;
    gy = gyro.y;
    gz = gyro.z;
    return true;
}