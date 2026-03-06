#include "time_series_normalizer.h"
#include <algorithm>
#include <vector>
#include <cmath>

/**
 * @brief 线性插值函数
 *
 * @param x0 起始x坐标
 * @param y0 起始y值
 * @param x1 结束x坐标
 * @param y1 结束y值
 * @param x 目标x坐标
 * @return 插值后的y值
 */
static float linear_interpolate(float x0, float y0, float x1, float y1, float x_val) {
    if (x0 == x1) {
        return y0;  // 避免除零
    }
    return y0 + (y1 - y0) * (x_val - x0) / (x1 - x0);
}

std::vector<std::vector<float>> time_normalize_sequence(
    const std::vector<std::vector<float>>& sequence,
    size_t target_length) {

    if (sequence.empty()) {
        return std::vector<std::vector<float>>(target_length, std::vector<float>());
    }

    size_t sequence_length = sequence.size();
    size_t num_features = sequence[0].size();

    // 如果序列长度等于目标长度，直接返回原序列
    if (sequence_length == target_length) {
        return sequence;
    }
    // 如果序列长度小于50，返回全零矩阵
    else if (sequence_length < 50) {
        return std::vector<std::vector<float>>(target_length, std::vector<float>(num_features, 0.0f));
    }
    // 否则，使用线性插值进行归一化
    else {
        // 创建原始序列和目标序列的时间轴 (范围从0到1)
        std::vector<float> original_indices(sequence_length);
        std::vector<float> target_indices(target_length);

        for (size_t i = 0; i < sequence_length; ++i) {
            original_indices[i] = static_cast<float>(i) / static_cast<float>(sequence_length - 1);
        }

        for (size_t i = 0; i < target_length; ++i) {
            target_indices[i] = static_cast<float>(i) / static_cast<float>(target_length - 1);
        }

        // 对每个特征维度分别进行线性插值
        std::vector<std::vector<float>> normalized_sequence(target_length, std::vector<float>(num_features));

        for (size_t feature_idx = 0; feature_idx < num_features; ++feature_idx) {
            for (size_t target_idx = 0; target_idx < target_length; ++target_idx) {
                float target_pos = target_indices[target_idx];

                // 找到目标位置在原始序列中的对应区间
                if (target_pos <= original_indices[0]) {
                    // 如果目标位置在序列开始之前，使用第一个值
                    normalized_sequence[target_idx][feature_idx] = sequence[0][feature_idx];
                } else if (target_pos >= original_indices.back()) {
                    // 如果目标位置在序列结束之后，使用最后一个值
                    normalized_sequence[target_idx][feature_idx] = sequence.back()[feature_idx];
                } else {
                    // 找到合适的插值区间
                    size_t left_idx = 0;
                    for (size_t i = 0; i < original_indices.size() - 1; ++i) {
                        if (original_indices[i] <= target_pos && target_pos <= original_indices[i + 1]) {
                            left_idx = i;
                            break;
                        }
                    }

                    size_t right_idx = left_idx + 1;

                    // 执行线性插值
                    normalized_sequence[target_idx][feature_idx] = linear_interpolate(
                        original_indices[left_idx], sequence[left_idx][feature_idx],
                        original_indices[right_idx], sequence[right_idx][feature_idx],
                        target_pos
                    );
                }
            }
        }

        return normalized_sequence;
    }
}

std::vector<std::vector<float>> normalize_mpu6050_data(
    const std::vector<std::vector<float>>& sequence,
    size_t target_length) {

    // 验证输入数据格式（应为6维特征向量）
    if (!sequence.empty() && sequence[0].size() != 6) {
        // 如果不是6维数据，返回空结果或抛出异常
        return std::vector<std::vector<float>>();
    }

    return time_normalize_sequence(sequence, target_length);
}