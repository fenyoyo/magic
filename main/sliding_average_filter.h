#ifndef _SLIDING_AVERAGE_FILTER_H_
#define _SLIDING_AVERAGE_FILTER_H_

#include <vector>
#include <cstddef>
#include <memory>

// #define USE_KALMAN_FILTER 1
/**
 * 滑动平均滤波器模板类
 * 对指定大小的数据窗口进行平均值计算
 */
template <typename T>
class SlidingAverageFilter
{
public:
    /**
     * 构造函数
     * @param window_size 窗口大小（参与平均计算的数据点数量）
     */
    explicit SlidingAverageFilter(size_t window_size);

    /**
     * 添加新数据点并返回当前平均值
     * @param value 新的数据点
     * @return 当前窗口内的平均值
     */
    T addValue(T value);

    /**
     * 获取当前平均值（不添加新数据）
     * @return 当前窗口内的平均值
     */
    T getAverage() const;

    /**
     * 重置滤波器
     */
    void reset();

    /**
     * 获取窗口大小
     * @return 窗口大小
     */
    size_t getWindowSize() const { return m_window_size; }

    /**
     * 获取当前缓冲区中有效数据的数量
     * @return 有效数据数量
     */
    size_t getValidCount() const { return m_count; }

private:
    std::vector<T> m_buffer; // 存储数据的缓冲区
    size_t m_window_size;    // 窗口大小
    size_t m_index;          // 当前写入位置
    size_t m_count;          // 当前缓冲区中的有效数据数量
    T m_sum;                 // 当前缓冲区中所有值的总和
};

/**
 * 滤波器类型枚举
 */
enum class FilterType
{
    SLIDING_AVERAGE, // 滑动平均滤波
    KALMAN_FILTER    // 卡尔曼滤波
};

// 前向声明卡尔曼滤波器类
namespace gamic
{
    class MPU6050KalmanFilter;
}

/**
 * MPU6050传感器数据滤波器
 * 支持多种滤波算法
 */
class MPU6050DataFilter
{
public:
    /**
     * 构造函数
     * @param window_size 滤波窗口大小（仅用于滑动平均滤波）
     * @param filter_type 滤波器类型
     */
    explicit MPU6050DataFilter(size_t window_size, FilterType filter_type = FilterType::SLIDING_AVERAGE);

    /**
     * 对MPU6050数据进行滤波处理
     * @param raw_data 原始传感器数据
     * @param dt 时间间隔（秒，仅用于卡尔曼滤波）
     * @return 滤波后的数据
     */
    struct MPU6050Data filterData(const struct MPU6050Data &raw_data, float dt = 0.02f);

    /**
     * 重置滤波器
     */
    void reset();

    /**
     * 设置滤波器类型
     */
    void setFilterType(FilterType filter_type);

    /**
     * 获取当前滤波器类型
     */
    FilterType getFilterType() const { return m_filter_type; }

    /**
     * 设置卡尔曼滤波器参数
     * @param gyro_noise 陀螺仪噪声
     * @param accel_noise 加速度计噪声
     */
    void setKalmanParameters(float gyro_noise, float accel_noise);

    /**
     * 设置滑动平均滤波器窗口大小
     * @param window_size 窗口大小
     */
    void setWindowSize(size_t window_size);

private:
    FilterType m_filter_type;
    size_t m_window_size;

    // 滑动平均滤波器组件
    SlidingAverageFilter<float> m_accel_x_filter;
    SlidingAverageFilter<float> m_accel_y_filter;
    SlidingAverageFilter<float> m_accel_z_filter;
    SlidingAverageFilter<float> m_gyro_x_filter;
    SlidingAverageFilter<float> m_gyro_y_filter;
    SlidingAverageFilter<float> m_gyro_z_filter;
    SlidingAverageFilter<float> m_temp_filter;

// 包含卡尔曼滤波器的头文件
#ifdef USE_KALMAN_FILTER
    std::unique_ptr<gamic::MPU6050KalmanFilter> m_kalman_filter;
#endif
};

#endif // _SLIDING_AVERAGE_FILTER_H_