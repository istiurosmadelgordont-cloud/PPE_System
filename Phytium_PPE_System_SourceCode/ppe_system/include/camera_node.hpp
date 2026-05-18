/**
 * @file      camera_node.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

/**
 * @brief 摄像头采集线程函数
 * 负责：初始化硬件、绑核CPU0、设置实时优先级、将图像推入 cap_queue
 */
void camera_thread_func();