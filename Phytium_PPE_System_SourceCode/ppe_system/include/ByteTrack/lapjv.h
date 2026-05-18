/**
 * @file      lapjv.h
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

#include <cstddef>

namespace byte_track {
int lapjv_internal(const size_t n, double *cost[], int *x, int *y);
}