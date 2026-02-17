#include <iostream>
#include <vector>
#include "sliding_average_filter.h"

int main() {
    // 测试滑动平均滤波器
    std::cout << "Testing Sliding Average Filter...\n";

    // 创建一个窗口大小为3的滤波器
    SlidingAverageFilter<float> filter(3);

    // 输入一系列带噪声的数据
    std::vector<float> noisy_data = {1.0, 1.2, 0.8, 1.1, 0.9, 3.0, 1.05, 0.95, 1.02};
    
    std::cout << "Input data: ";
    for (float val : noisy_data) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    std::cout << "Filtered data: ";
    for (float val : noisy_data) {
        float filtered_val = filter.addValue(val);
        std::cout << filtered_val << " ";
    }
    std::cout << "\n";

    // 测试MPU6050数据滤波器
    std::cout << "\nTesting MPU6050 Data Filter...\n";
    
    MPU6050DataFilter data_filter(3);
    
    // 模拟几组带噪声的传感器数据
    for (int i = 0; i < 5; i++) {
        struct MPU6050Data raw_data;
        raw_data.accel_x = 0.1 + static_cast<float>(i) * 0.01;  // 模拟小幅度变化
        raw_data.accel_y = 0.05 + static_cast<float>(i) * 0.02;
        raw_data.accel_z = 1.0 + static_cast<float>(i) * 0.005;
        raw_data.gyro_x = static_cast<float>(rand()) / RAND_MAX * 0.1;  // 模拟小幅度噪声
        raw_data.gyro_y = static_cast<float>(rand()) / RAND_MAX * 0.1;
        raw_data.gyro_z = static_cast<float>(rand()) / RAND_MAX * 0.1;
        raw_data.temperature = 25.0f;
        
        struct MPU6050Data filtered_data = data_filter.filterData(raw_data);
        
        std::cout << "Raw: ax=" << raw_data.accel_x << ", ay=" << raw_data.accel_y 
                  << ", az=" << raw_data.accel_z << ", gx=" << raw_data.gyro_x 
                  << ", gy=" << raw_data.gyro_y << ", gz=" << raw_data.gyro_z << "\n";
        std::cout << "Filtered: ax=" << filtered_data.accel_x << ", ay=" << filtered_data.accel_y 
                  << ", az=" << filtered_data.accel_z << ", gx=" << filtered_data.gyro_x 
                  << ", gy=" << filtered_data.gyro_y << ", gz=" << filtered_data.gyro_z << "\n\n";
    }

    return 0;
}