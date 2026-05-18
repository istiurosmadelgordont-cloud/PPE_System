#pragma once
#include "core_config.hpp"
#include "lockfree_queue.hpp"
#include <atomic>
#include <string>

// ==========================================
// 全局跨线程共享状态（集中声明，消灭散布的 extern）
// ==========================================

// 系统运行开关
// 【优化 3】：多线程共享的 bool 必须使用 atomic，防止 ARM 多核缓存不一致
extern std::atomic<bool> is_running;

// 无锁环形缓冲区
// 视频帧缓冲设为 5（防爆显存），报警事件缓冲设为 20（防证据丢失）
extern LockFreeRingBuffer<cv::Mat, 5> cap_queue;
extern LockFreeRingBuffer<AlarmEvent, 20> alarm_queue;

// 媒体控制变量
// 【优化 3】：source_changed 也被多线程读写，必须 atomic
extern std::atomic<int> current_source_mode;
extern std::string video_path;
extern std::atomic<bool> source_changed;