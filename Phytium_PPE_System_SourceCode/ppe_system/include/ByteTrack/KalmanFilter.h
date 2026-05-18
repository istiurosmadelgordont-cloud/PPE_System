/**
 * @file      KalmanFilter.h
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

#include "Eigen/Dense"

#include "ByteTrack/Rect.h"

namespace byte_track {
class KalmanFilter {
public:
  using DetectBox = Xyah<float>;

  using StateMean = Eigen::Matrix<float, 1, 8, Eigen::RowMajor>;
  using StateCov = Eigen::Matrix<float, 8, 8, Eigen::RowMajor>;

  using StateHMean = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;
  using StateHCov = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

  KalmanFilter(const float &std_weight_position = 1. / 20,
               const float &std_weight_velocity = 1. / 160);

  void initiate(StateMean &mean, StateCov &covariance,
                const DetectBox &measurement);

  void predict(StateMean &mean, StateCov &covariance);

  void update(StateMean &mean, StateCov &covariance,
              const DetectBox &measurement);

private:
  float std_weight_position_;
  float std_weight_velocity_;

  Eigen::Matrix<float, 8, 8, Eigen::RowMajor> motion_mat_;
  Eigen::Matrix<float, 4, 8, Eigen::RowMajor> update_mat_;

  void project(StateHMean &projected_mean, StateHCov &projected_covariance,
               const StateMean &mean, const StateCov &covariance);
};
} // namespace byte_track