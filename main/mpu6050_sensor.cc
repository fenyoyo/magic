#include "mpu6050_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char *MPU6050Sensor::TAG = "MPU6050Sensor";
MPU6050Sensor *MPU6050Sensor::s_instance = nullptr;

MPU6050Sensor::MPU6050Sensor()
    : m_initialized(false), m_sampling(false), m_sampling_interval_ms(100), m_calibrated(false), m_task_handle(nullptr)
{
    ESP_ERROR_CHECK(i2cdev_init());
}

MPU6050Sensor::~MPU6050Sensor()
{
    stopSampling();
    s_instance = nullptr;
}

MPU6050Sensor &MPU6050Sensor::getInstance()
{
    if (s_instance == nullptr)
    {
        s_instance = new MPU6050Sensor();
    }
    return *s_instance;
}

bool MPU6050Sensor::checkDevicePresence()
{
    int retry_count = 0;
    const int max_retries = 10;

    while (retry_count < max_retries)
    {
        esp_err_t res = i2c_dev_probe(&m_dev.i2c_dev, I2C_DEV_WRITE);
        if (res == ESP_OK)
        {
            ESP_LOGI(TAG, "Found MPU60x0 device");
            return true;
        }

        ESP_LOGW(TAG, "MPU60x0 not found (attempt %d/%d)",
                 retry_count + 1, max_retries);

        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }

    ESP_LOGE(TAG, "MPU60x0 device not found after %d attempts", max_retries);
    return false;
}

bool MPU6050Sensor::initDevice()
{
    // 初始化设备描述符
    esp_err_t ret = mpu6050_init_desc(&m_dev, ADDR, static_cast<i2c_port_t>(0), static_cast<gpio_num_t>(CONFIG_EXAMPLE_SDA_GPIO), static_cast<gpio_num_t>(CONFIG_EXAMPLE_SCL_GPIO));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize device descriptor: %s", esp_err_to_name(ret));
        return false;
    }

    // 检查设备是否存在
    if (!checkDevicePresence())
    {
        return false;
    }

    // 初始化MPU6050
    ret = mpu6050_init(&m_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MPU6050: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    ESP_LOGI(TAG, "Accel range: %d", m_dev.ranges.accel);
    ESP_LOGI(TAG, "Gyro range:  %d", m_dev.ranges.gyro);

    return true;
}

bool MPU6050Sensor::begin(uint8_t addr, int sda_pin, int scl_pin)
{

    if (m_initialized)
    {
        ESP_LOGW(TAG, "MPU6050 already initialized");
        return true;
    }

    m_device_addr = addr;
    m_sda_pin = sda_pin;
    m_scl_pin = scl_pin;

    m_initialized = initDevice();
    return m_initialized;
}

void MPU6050Sensor::calibrateGyro(int samples)
{
    if (!m_initialized)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting gyro calibration with %d samples...", samples);

    // 清零偏移量
    m_gyro_offset.x = 0;
    m_gyro_offset.y = 0;
    m_gyro_offset.z = 0;

    // 等待传感器稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 采集样本
    for (int i = 0; i < samples; i++)
    {
        mpu6050_acceleration_t accel;
        mpu6050_rotation_t gyro;

        if (mpu6050_get_motion(&m_dev, &accel, &gyro) == ESP_OK)
        {
            m_gyro_offset.x += gyro.x;
            m_gyro_offset.y += gyro.y;
            m_gyro_offset.z += gyro.z;
        }

        // 显示进度
        if ((i + 1) % (samples / 10) == 0)
        {
            ESP_LOGI(TAG, "Calibration progress: %d%%", (i + 1) * 100 / samples);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // 计算平均值
    m_gyro_offset.x /= samples;
    m_gyro_offset.y /= samples;
    m_gyro_offset.z /= samples;

    m_calibrated = true;

    ESP_LOGI(TAG, "Gyro calibration completed");
    ESP_LOGI(TAG, "Gyro offsets: x=%.4f, y=%.4f, z=%.4f",
             m_gyro_offset.x, m_gyro_offset.y, m_gyro_offset.z);
}

bool MPU6050Sensor::readData(mpu6050_acceleration_t &accel,
                             mpu6050_rotation_t &gyro,
                             float &temperature)
{
    if (!m_initialized)
    {
        return false;
    }

    // 读取温度
    esp_err_t ret = mpu6050_get_temperature(&m_dev, &temperature);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(ret));
        return false;
    }

    // 读取运动数据
    ret = mpu6050_get_motion(&m_dev, &accel, &gyro);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read motion data: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

mpu6050_rotation_t MPU6050Sensor::getCalibratedGyro()
{
    mpu6050_rotation_t gyro = {};
    mpu6050_acceleration_t accel = {};
    float temp;

    if (readData(accel, gyro, temp))
    {
        if (m_calibrated)
        {
            gyro.x -= m_gyro_offset.x;
            gyro.y -= m_gyro_offset.y;
            gyro.z -= m_gyro_offset.z;
        }
    }

    return gyro;
}

void MPU6050Sensor::samplingTask(void *pvParameters)
{
    MPU6050Sensor *sensor = static_cast<MPU6050Sensor *>(pvParameters);

    ESP_LOGI(TAG, "Sampling task started");

    while (sensor->m_sampling)
    {
        mpu6050_acceleration_t accel = {};
        mpu6050_rotation_t gyro = {};
        float temperature = 0;

        if (sensor->readData(accel, gyro, temperature))
        {
            // 应用陀螺仪校准
            if (sensor->m_calibrated)
            {
                gyro.x -= sensor->m_gyro_offset.x;
                gyro.y -= sensor->m_gyro_offset.y;
                gyro.z -= sensor->m_gyro_offset.z;
            }

            // 触发回调
            if (sensor->m_data_callback)
            {
                sensor->m_data_callback(accel, gyro, temperature);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(sensor->m_sampling_interval_ms));
    }

    ESP_LOGI(TAG, "Sampling task stopped");
    vTaskDelete(NULL);
}

void MPU6050Sensor::startSampling(int interval_ms)
{
    if (!m_initialized)
    {
        ESP_LOGE(TAG, "Cannot start sampling: device not initialized");
        return;
    }

    if (m_sampling)
    {
        ESP_LOGW(TAG, "Sampling already running");
        return;
    }

    m_sampling_interval_ms = interval_ms;
    m_sampling = true;

    // 创建采样任务
    BaseType_t ret = xTaskCreatePinnedToCore(
        samplingTask,
        "mpu6050_task",
        4096,
        this,
        5,
        &m_task_handle,
        tskNO_AFFINITY);

    if (ret == pdPASS)
    {
        ESP_LOGI(TAG, "Sampling task created, interval: %d ms", interval_ms);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create sampling task");
        m_sampling = false;
    }
}

void MPU6050Sensor::stopSampling()
{
    if (m_sampling)
    {
        m_sampling = false;

        if (m_task_handle != nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelete(m_task_handle);
            m_task_handle = nullptr;
        }

        ESP_LOGI(TAG, "Sampling stopped");
    }
}