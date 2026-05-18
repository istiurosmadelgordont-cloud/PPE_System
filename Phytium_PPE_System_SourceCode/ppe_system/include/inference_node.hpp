/**
 * @file      inference_node.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#include <thread> // 必须加上这一行，否则不认识 yield()
#pragma once

/**
 * @brief AI 推理与去抖状态机线程函数
 * 负责：加载NCNN模型、绑核CPU2&3、从缓存取图、执行YOLO推理、逻辑去抖、生成报警
 */
void inference_thread_func();