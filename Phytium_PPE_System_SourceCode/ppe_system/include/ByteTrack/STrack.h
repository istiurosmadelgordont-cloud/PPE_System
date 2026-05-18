/**
 * @file      STrack.h
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

#include "ByteTrack/KalmanFilter.h"
#include "ByteTrack/Rect.h"

#include <cstddef>

namespace byte_track {
enum class STrackState {
  New = 0,
  Tracked = 1,
  Lost = 2,
  Removed = 3,
};

class STrack {
public:
  STrack(const Rect<float> &rect, const float &score);
  ~STrack();

  const Rect<float> &getRect() const;
  const STrackState &getSTrackState() const;

  const bool &isActivated() const;
  const float &getScore() const;
  const size_t &getTrackId() const;
  const size_t &getFrameId() const;
  const size_t &getStartFrameId() const;
  const size_t &getTrackletLength() const;

  void activate(const size_t &frame_id, const size_t &track_id);
  void reActivate(const STrack &new_track, const size_t &frame_id,
                  const int &new_track_id = -1);

  void predict();
  void update(const STrack &new_track, const size_t &frame_id);

  void markAsLost();
  void markAsRemoved();

private:
  KalmanFilter kalman_filter_;
  KalmanFilter::StateMean mean_;
  KalmanFilter::StateCov covariance_;

  Rect<float> rect_;
  STrackState state_;

  bool is_activated_;
  float score_;
  size_t track_id_;
  size_t frame_id_;
  size_t start_frame_id_;
  size_t tracklet_len_;

  void updateRect();
};
} // namespace byte_track