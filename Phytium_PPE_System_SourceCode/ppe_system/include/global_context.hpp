#pragma once
#include "core_config.hpp"
#include "lockfree_queue.hpp" 

// 将互斥锁队列替换为极致性能的无锁环形缓冲区
// 视频帧缓冲设为 5（防爆显存），报警事件缓冲设为 20（防证据丢失）
extern LockFreeRingBuffer<cv::Mat, 5> cap_queue;       
extern LockFreeRingBuffer<AlarmEvent, 20> alarm_queue;  
extern bool is_running;