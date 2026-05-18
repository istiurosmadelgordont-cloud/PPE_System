/**
 * @file      Rect.h
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

#include "Eigen/Dense"

namespace byte_track {
template <typename T> using Tlwh = Eigen::Matrix<T, 1, 4, Eigen::RowMajor>;

template <typename T> using Tlbr = Eigen::Matrix<T, 1, 4, Eigen::RowMajor>;

template <typename T> using Xyah = Eigen::Matrix<T, 1, 4, Eigen::RowMajor>;

template <typename T> class Rect {
public:
  Tlwh<T> tlwh;

  Rect() = default;
  Rect(const T &x, const T &y, const T &width, const T &height);

  ~Rect();

  const T &x() const;
  const T &y() const;
  const T &width() const;
  const T &height() const;

  T &x();
  T &y();
  T &width();
  T &height();

  const T &tl_x() const;
  const T &tl_y() const;
  T br_x() const;
  T br_y() const;

  Tlbr<T> getTlbr() const;
  Xyah<T> getXyah() const;

  float calcIoU(const Rect<T> &other) const;
};

template <typename T> Rect<T> generate_rect_by_tlbr(const Tlbr<T> &tlbr);

template <typename T> Rect<T> generate_rect_by_xyah(const Xyah<T> &xyah);

} // namespace byte_track