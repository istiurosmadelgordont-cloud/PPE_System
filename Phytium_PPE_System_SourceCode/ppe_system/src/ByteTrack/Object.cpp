/**
 * @file      Object.cpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#include "ByteTrack/Object.h"

byte_track::Object::Object(const Rect<float> &_rect, const int &_label,
                           const float &_prob)
    : rect(_rect), label(_label), prob(_prob) {}