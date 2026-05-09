/**
 * @file      global_context.cpp
 * @brief     全局上下文与跨线程状态同步管理
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-16
 * @copyright Copyright (c) 2026 [双生序章]. All rights reserved.
 */
#include "global_context.hpp"
#include <string>

//1.实例化全局开关
bool is_running = true;

//2.实例化全局无锁队列(必须和hpp里的类型与大小严格对应)
LockFreeRingBuffer<cv::Mat, 5> cap_queue;
LockFreeRingBuffer<AlarmEvent, 20> alarm_queue;

//3.实例化媒体控制变量
int current_source_mode = 0; 
std::string video_path = "";
bool source_changed = false;
