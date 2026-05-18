/**
 * @file      global_context.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once
#include "core_config.hpp"
#include "lockfree_queue.hpp"
#include <atomic>
#include <string>

// ==========================================
// 跨核心与多线程通信的数据总线上下文
// ==========================================

/// @brief
/// 全局系统运行标识位，修改此原子变量可安全且平滑地退出各计算核心的独立线程
extern std::atomic<bool> is_running;

// ==========================================
// 异构系统高性能无锁环形缓冲区定义
// ==========================================

/// @brief [Core -> GPU/NPU] 视频推流队列。缓冲区设为 5
/// 帧，严格控制内存占用，防止爆显存
extern LockFreeRingBuffer<cv::Mat, 5> cap_queue;

/// @brief [NPU -> Core] AI 推理报警事件队列。设为 20
/// 深度，防止突发多并发违规导致监控日志丢失
extern LockFreeRingBuffer<AlarmEvent, 20> alarm_queue;

// ==========================================
// 多模态热更新与流媒体控制字
// ==========================================

/// @brief 视频源挂载模式：0 为硬件摄像头直连采集，1 为本地 MP4 离线视频分析
extern std::atomic<int> current_source_mode;

/// @brief 媒体流回放绝对路径，当 current_source_mode == 1
/// 时，解码器从此处拉取视频数据
extern std::string video_path;

/// @brief 动态源切换锁存器，当用户在 UI
/// 进行热切换时拉高此标识，触发底层流媒体管道的重建
extern std::atomic<bool> source_changed;