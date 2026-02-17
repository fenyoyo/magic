#include "sliding_average_filter.h"
#include "mpu6050_sensor.h"
#include <numeric>
#include "esp_log.h"

// 包含卡尔曼滤波器头文件
#ifdef USE_KALMAN_FILTER
#include "mpu6050_kalman_filter.h"
#endif
#define TAG "SlidingAverageFilter"

// SlidingAverageFilter 模板类实现
template <typename T>
SlidingAverageFilter<T>::SlidingAverageFilter(size_t window_size)
    : m_buffer(window_size), m_window_size(window_size), m_index(0), m_count(0), m_sum(0)
{
}

template <typename T>
T SlidingAverageFilter<T>::addValue(T value)
{
    if (m_window_size == 0)
    {
        return value;
    }

    // 如果缓冲区还没填满，直接添加
    if (m_count < m_window_size)
    {
        m_buffer[m_index] = value;
        m_sum += value;
        m_count++;
    }
    else
    {
        // 缓冲区已满，替换最旧的值
        T old_value = m_buffer[m_index];
        m_buffer[m_index] = value;
        m_sum = m_sum - old_value + value;
    }

    // 更新索引，循环使用缓冲区
    m_index = (m_index + 1) % m_window_size;

    // 计算平均值
    return m_sum / static_cast<T>(m_count);
}

template <typename T>
T SlidingAverageFilter<T>::getAverage() const
{
    if (m_count == 0)
    {
        return T{};
    }
    return m_sum / static_cast<T>(m_count);
}

template <typename T>
void SlidingAverageFilter<T>::reset()
{
    m_index = 0;
    m_count = 0;
    m_sum = T{};
    // 将缓冲区清零
    for (auto &val : m_buffer)
    {
        val = T{};
    }
}

// 为常用类型显式实例化模板
template class SlidingAverageFilter<float>;
template class SlidingAverageFilter<double>;

// MPU6050DataFilter 实现
MPU6050DataFilter::MPU6050DataFilter(size_t window_size, FilterType filter_type)
    : m_filter_type(filter_type), m_window_size(window_size),
      m_accel_x_filter(window_size), m_accel_y_filter(window_size), m_accel_z_filter(window_size),
      m_gyro_x_filter(window_size), m_gyro_y_filter(window_size), m_gyro_z_filter(window_size),
      m_temp_filter(window_size)
{
#ifdef USE_KALMAN_FILTER
    if (m_filter_type == FilterType::KALMAN_FILTER)
    {
        m_kalman_filter = std::make_unique<gamic::MPU6050KalmanFilter>();
    }
    ESP_LOGI(TAG, "MPU6050DataFilter init");
#endif
    ESP_LOGI(TAG, "MPU6050DataFilter");
}

struct MPU6050Data MPU6050DataFilter::filterData(const struct MPU6050Data &raw_data, float dt)
{
    switch (m_filter_type)
    {
    case FilterType::SLIDING_AVERAGE:
    {
        struct MPU6050Data filtered_data;

        filtered_data.accel_x = m_accel_x_filter.addValue(raw_data.accel_x);
        filtered_data.accel_y = m_accel_y_filter.addValue(raw_data.accel_y);
        filtered_data.accel_z = m_accel_z_filter.addValue(raw_data.accel_z);

        filtered_data.gyro_x = m_gyro_x_filter.addValue(raw_data.gyro_x);
        filtered_data.gyro_y = m_gyro_y_filter.addValue(raw_data.gyro_y);
        filtered_data.gyro_z = m_gyro_z_filter.addValue(raw_data.gyro_z);

        filtered_data.temperature = m_temp_filter.addValue(raw_data.temperature);

        return filtered_data;
    }
#ifdef USE_KALMAN_FILTER
    case FilterType::KALMAN_FILTER:
    {
        if (m_kalman_filter)
        {
            return m_kalman_filter->filterData(raw_data, dt);
        }
        // 如果卡尔曼滤波器未初始化，回退到滑动平均
        struct MPU6050Data filtered_data;
        filtered_data.accel_x = m_accel_x_filter.addValue(raw_data.accel_x);
        filtered_data.accel_y = m_accel_y_filter.addValue(raw_data.accel_y);
        filtered_data.accel_z = m_accel_z_filter.addValue(raw_data.accel_z);
        filtered_data.gyro_x = m_gyro_x_filter.addValue(raw_data.gyro_x);
        filtered_data.gyro_y = m_gyro_y_filter.addValue(raw_data.gyro_y);
        filtered_data.gyro_z = m_gyro_z_filter.addValue(raw_data.gyro_z);
        filtered_data.temperature = m_temp_filter.addValue(raw_data.temperature);
        return filtered_data;
    }
#endif
    default:
    {
        // 默认使用滑动平均滤波
        struct MPU6050Data filtered_data;

        filtered_data.accel_x = m_accel_x_filter.addValue(raw_data.accel_x);
        filtered_data.accel_y = m_accel_y_filter.addValue(raw_data.accel_y);
        filtered_data.accel_z = m_accel_z_filter.addValue(raw_data.accel_z);

        filtered_data.gyro_x = m_gyro_x_filter.addValue(raw_data.gyro_x);
        filtered_data.gyro_y = m_gyro_y_filter.addValue(raw_data.gyro_y);
        filtered_data.gyro_z = m_gyro_z_filter.addValue(raw_data.gyro_z);

        filtered_data.temperature = m_temp_filter.addValue(raw_data.temperature);

        return filtered_data;
    }
    }
}

void MPU6050DataFilter::reset()
{
    switch (m_filter_type)
    {
    case FilterType::SLIDING_AVERAGE:
    {
        m_accel_x_filter.reset();
        m_accel_y_filter.reset();
        m_accel_z_filter.reset();
        m_gyro_x_filter.reset();
        m_gyro_y_filter.reset();
        m_gyro_z_filter.reset();
        m_temp_filter.reset();
        break;
    }
#ifdef USE_KALMAN_FILTER
    case FilterType::KALMAN_FILTER:
    {
        if (m_kalman_filter)
        {
            m_kalman_filter->reset();
        }
        break;
    }
#endif
    default:
    {
        m_accel_x_filter.reset();
        m_accel_y_filter.reset();
        m_accel_z_filter.reset();
        m_gyro_x_filter.reset();
        m_gyro_y_filter.reset();
        m_gyro_z_filter.reset();
        m_temp_filter.reset();
        break;
    }
    }
}

void MPU6050DataFilter::setFilterType(FilterType filter_type)
{
    if (m_filter_type != filter_type)
    {
        m_filter_type = filter_type;

#ifdef USE_KALMAN_FILTER
        if (m_filter_type == FilterType::KALMAN_FILTER && !m_kalman_filter)
        {
            m_kalman_filter = std::make_unique<gamic::MPU6050KalmanFilter>();
        }
#endif
    }
}

void MPU6050DataFilter::setKalmanParameters(float gyro_noise, float accel_noise)
{
#ifdef USE_KALMAN_FILTER
    if (m_filter_type == FilterType::KALMAN_FILTER && m_kalman_filter)
    {
        m_kalman_filter->setProcessNoise(gyro_noise, accel_noise);
        m_kalman_filter->setMeasurementNoise(gyro_noise, accel_noise);
    }
#endif
}

void MPU6050DataFilter::setWindowSize(size_t window_size)
{
    if (m_filter_type == FilterType::SLIDING_AVERAGE)
    {
        // 重新创建滑动平均滤波器
        m_accel_x_filter = SlidingAverageFilter<float>(window_size);
        m_accel_y_filter = SlidingAverageFilter<float>(window_size);
        m_accel_z_filter = SlidingAverageFilter<float>(window_size);
        m_gyro_x_filter = SlidingAverageFilter<float>(window_size);
        m_gyro_y_filter = SlidingAverageFilter<float>(window_size);
        m_gyro_z_filter = SlidingAverageFilter<float>(window_size);
        m_temp_filter = SlidingAverageFilter<float>(window_size);
        m_window_size = window_size;
    }
}