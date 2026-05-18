/**
 * @file      global_context.cpp
 * @brief     全局上下文与跨线程状态同步管理
 * @author    [双生序章]
 * @version   2.1.0
 * @date      2026-05-18
 * @copyright Copyright (c) 2026 [双生序章]. All rights reserved.
 */
#include "global_context.hpp"

/// @brief 实例化全局生命周期开关（基于 atomic 内存屏障保证多核可见性）
std::atomic<bool> is_running{true};

/// @brief 实例化跨核无锁环形队列内存区，建立进程内极速通信管道
LockFreeRingBuffer<cv::Mat, 5> cap_queue;
LockFreeRingBuffer<AlarmEvent, 20> alarm_queue;

/// @brief 实例化流媒体状态控制字，保障 UI 线程与采集线程的热更新一致性
std::atomic<int> current_source_mode{0};
std::string video_path = "";
std::atomic<bool> source_changed{false};
