/**
 * @file      camera_node.cpp
 * @brief     视频拉流与硬件采集守护节点
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-05-01
 * @copyright Copyright (c) 2026 [双生序章]. All rights reserved.
 * @note      本模块负责绑定小核处理视频流解码，包含物理摄像头与本地文件的无缝热切换机制。
 */
#include "camera_node.hpp"
#include "global_context.hpp"
#include "core_config.hpp"
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdio.h>

extern bool is_running;
extern int current_source_mode; 
extern std::string video_path;
extern bool source_changed;

void camera_thread_func() {
    // 1.绑核CPU0(FTC310 小核)
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(CORE_CAM, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // 2.尝试打开采集源
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    // [注意]：不要在这里死磕 set 参数，如果没打开，set 会引发底层异常
    if (cap.isOpened()) {
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30); 
    } else {
        printf("\n🚨 [Camera] 警告：物理摄像头已被底层(OpenAMP)占用或分配内存失败！\n");
        printf("👉 系统已进入休眠轮询模式，请在 UI 界面点击【导入录像】进行离线分析。\n");
        // 【关键修改】：绝对不要写 is_running = false; 也不要 return; 必须让线程活下去！
    }

    cv::Mat tmp_frame;
    while (is_running) {
        // 模式切换检测
        if (source_changed) {
            if (cap.isOpened()) {
                cap.release();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (current_source_mode == 0) {
                if(cap.open(0, cv::CAP_V4L2)) {
                    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
                    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
                    cap.set(cv::CAP_PROP_FPS, 30); 
                    printf("📷 系统已切换至：实时监控模式\n");
                } else {
                    printf("❌ [致命错误] 实时摄像头依然无法打开，请检查底层资源！\n");
                }
            } else if (current_source_mode == 1) {

                if(cap.open(video_path)) {
                    printf("🎬 系统已切换至：视频分析模式 (%s)\n", video_path.c_str());
                } else {
                    printf("❌ [致命错误] 视频文件打开失败！OpenCV 缺少解码器或路径错误！\n");
                    // 自动退回，防止后续拉取空帧
                    current_source_mode = 0;
                }
            }
            source_changed = false;
        }

        // 【新增防呆保护】：如果当前无论是摄像头还是视频都没打开成功，直接空转等待
        if (!cap.isOpened()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        cap >> tmp_frame;
        // ... （下方 tmp_frame.empty() 的逻辑保持你原来的不变）...
        if (tmp_frame.empty()) {
            if (current_source_mode == 1) {
                printf("✅ 视频分析完毕，正在自动切回实时监控...\n");
                current_source_mode = 0; 
                source_changed = true;   
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue; 
            }
            continue;
        }

        if (current_source_mode == 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 导入视频时限速
        }

        //实时性管理：基于无锁队列的旧帧淘汰机制
        // 注意：摄像头抓取的底层 buffer 会复用，所以入队时必须 clone()
        if (!cap_queue.push(tmp_frame.clone())) {
        cv::Mat push_frame = tmp_frame.clone();
        if (!cap_queue.push(push_frame)) {
            cv::Mat trash;
            cap_queue.pop(trash);               // 
            cap_queue.push(tmp_frame.clone());  // 
            cap_queue.push(push_frame);         // 直接推入已克隆的帧，消灭双重拷贝
        }
    }
    cap.release();
}
