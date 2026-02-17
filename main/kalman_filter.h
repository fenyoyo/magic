#pragma once
#include <array>
#include <vector>
#include <algorithm>

namespace gamic {

/// 单变量状态估计的卡尔曼滤波器
/// @tparam N 状态数量
///
/// 此类实现了一个简单的卡尔曼滤波器，用于估计变量的状态，
/// 如系统的方向。滤波器基于以下状态空间模型：
/// \f[
/// X' = X + U \cdot dt
/// \f]
/// \f[
/// P' = P + Q
/// \f]
/// \f[
/// Y = Z - X
/// \f]
/// \f[
/// K = P / (P + R)
/// \f]
/// \f[
/// X = X + K \cdot Y
/// \f]
/// \f[
/// P = (1 - K) \cdot P
/// \f]
/// 其中:
/// - \f$X\f$ 是状态向量
/// - \f$P\f$ 是协方差矩阵
/// - \f$Q\f$ 是过程噪声
/// - \f$R\f$ 是测量噪声
/// - \f$U\f$ 是控制输入
/// - \f$dt\f$ 是时间步长
/// - \f$Z\f$ 是测量值
template <size_t N>
class KalmanFilter {
public:
  /// 构造函数
  KalmanFilter() {
    X.fill(0.0f); // 初始化状态向量
    P.fill({});   // 初始化协方差矩阵
    for (size_t i = 0; i < N; i++) {
      P[i][i] = 1.0f;  // 初始协方差
      Q[i][i] = 0.001f; // 过程噪声
      R[i][i] = 0.01f;  // 测量噪声
    }
  }

  /// 设置过程噪声
  /// @param q 过程噪声
  /// @note 过程噪声是模型中的不确定性。它用于计算误差协方差，
  /// 该协方差决定了对预测的信任程度。较高的过程噪声意味着
  /// 预测不太可信，滤波器更多地依赖于测量。较低的过程噪声
  /// 意味着预测更可信，滤波器较少依赖于测量。
  void set_process_noise(float q) {
    for (size_t i = 0; i < N; i++)
      Q[i][i] = q;
  }

  /// 设置过程噪声
  /// @param q 过程噪声向量
  void set_process_noise(const std::array<float, N> &q) {
    for (size_t i = 0; i < N; i++)
      Q[i][i] = q[i];
  }

  /// 设置测量噪声
  /// @param r 测量噪声
  /// @note 测量噪声是测量中的不确定性。它用于计算卡尔曼增益，
  /// 该增益确定测量对状态估计的影响程度。
  void set_measurement_noise(float r) {
    for (size_t i = 0; i < N; i++)
      R[i][i] = r;
  }

  /// 设置测量噪声
  /// @param r 测量噪声向量
  void set_measurement_noise(const std::array<float, N> &r) {
    for (size_t i = 0; i < N; i++)
      R[i][i] = r[i];
  }

  /// 预测下一状态
  /// @param U 控制输入
  /// @param dt 时间步长
  /// @note 预测步骤基于当前状态和控制输入估计下一时间步的状态。
  /// 控制输入用于模拟控制对状态的影响。时间步长是测量之间的时间。
  void predict(const std::array<float, N> &U, float dt) {
    // 预测下一状态: X' = X + U * dt
    for (size_t i = 0; i < N; i++) {
      X[i] += U[i] * dt;
    }
    // 更新协方差: P = P + Q
    for (size_t i = 0; i < N; i++) {
      for (size_t j = 0; j < N; j++) {
        P[i][j] += Q[i][j];
      }
    }
  }

  /// 更新状态估计
  /// @param Z 测量值
  /// @note 校正步骤基于测量更新状态估计。测量用于计算卡尔曼增益，
  /// 该增益确定测量对状态估计的影响程度。卡尔曼增益用于更新状态估计和
  /// 误差协方差。
  void update(const std::array<float, N> &Z) {
    std::array<float, N> Y; // 测量残差
    // 计算残差: Y = Z - X
    for (size_t i = 0; i < N; i++) {
      Y[i] = Z[i] - X[i];
    }
    // 计算卡尔曼增益: K = P / (P + R)
    for (size_t i = 0; i < N; i++) {
      for (size_t j = 0; j < N; j++) {
        K[i][j] = P[i][j] / (P[i][j] + R[i][j]);
      }
    }
    // 更新状态: X = X + K * Y
    for (size_t i = 0; i < N; i++) {
      X[i] += K[i][i] * Y[i];
    }
    // 更新误差协方差: P = (1 - K) * P
    for (size_t i = 0; i < N; i++) {
      for (size_t j = 0; j < N; j++) {
        P[i][j] = (1 - K[i][i]) * P[i][j];
      }
    }
  }

  /// 获取状态估计
  /// @return 状态估计
  /// @note 状态估计是基于测量的当前状态估计。
  const std::array<float, N> &get_state() const {
    return X;
  }

  /// 重置滤波器到初始状态
  void reset() {
    X.fill(0.0f);
    for (size_t i = 0; i < N; i++) {
      for (size_t j = 0; j < N; j++) {
        P[i][j] = (i == j) ? 1.0f : 0.0f;  // 对角线为1，其余为0
        Q[i][j] = (i == j) ? 0.001f : 0.0f; // 默认过程噪声
        R[i][j] = (i == j) ? 0.01f : 0.0f;  // 默认测量噪声
        K[i][j] = 0.0f;
      }
    }
  }

protected:
  std::array<float, N> X;                                    // 状态向量
  std::array<std::array<float, N>, N> P;                     // 协方差矩阵
  std::array<std::array<float, N>, N> Q;                     // 过程噪声
  std::array<std::array<float, N>, N> R;                     // 测量噪声
  std::array<std::array<float, N>, N> K;                     // 卡尔曼增益
};

} // namespace gamic