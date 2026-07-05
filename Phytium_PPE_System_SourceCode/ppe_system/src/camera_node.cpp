/**
 * @file      camera_node.cpp
 * @brief     视频拉流与硬件采集守护节点
 * @author    [双生序章]
 * @version   2.1.0
 * @date      2026-05-18
 * @copyright Copyright (c) 2026 [双生序章]. All rights reserved.
 * @note
 * 本模块负责绑定小核处理视频流解码，包含物理摄像头与本地文件的无缝热切换机制。
 */
#include "camera_node.hpp"
#include "core_config.hpp"
#include "global_context.hpp"
#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <stdio.h>
#include <thread>

void camera_thread_func() {
  // 1.绑核CPU0(FTC310 小核)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(CORE_CAM, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  // 2.尝试打开采集源
  cv::VideoCapture cap(0, cv::CAP_V4L2);
  // [注意]：不要在这里死磕 set 参数，如果没打开，set 会引发底层异常
  if (cap.isOpened()) {
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1); // 极致实时：设置缓冲区帧数为1，彻底消灭视频流排队滞后
  } else {
    printf("\n🚨 [Camera] "
           "警告：物理摄像头已被底层(OpenAMP)占用或分配内存失败！\n");
    printf("👉 系统已进入休眠轮询模式，请在 UI "
           "界面点击【导入录像】进行离线分析。\n");
    // 【关键修改】：绝对不要写 is_running = false; 也不要 return;
    // 必须让线程活下去！
  }

  cv::Mat tmp_frame;
  auto last_frame_time = std::chrono::steady_clock::now();
  while (is_running) {
    // 模式切换检测
    if (source_changed) {
      if (cap.isOpened()) {
        cap.release();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (current_source_mode == 0) {
        bool ok = false;
        if (cap.open(0, cv::CAP_V4L2)) {
          ok = true;
        } else if (cap.open(1, cv::CAP_V4L2)) {
          ok = true;
          printf("ℹ️ 默认 video0 打开失败，已自动切换至备用节点 video1\n");
        } else if (cap.open(2, cv::CAP_V4L2)) {
          ok = true;
          printf("ℹ️ 默认 video0/1 打开失败，已自动切换至备用节点 video2\n");
        }

        if (ok) {
          cap.set(cv::CAP_PROP_FOURCC,
                  cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
          cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
          cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
          cap.set(cv::CAP_PROP_FPS, 30);
          cap.set(cv::CAP_PROP_BUFFERSIZE, 1); // 极致实时
          printf("📷 系统已切换至：实时监控模式\n");
        } else {
          printf("❌ [致命错误] 实时摄像头(video0/1/2)依然无法打开，请检查底层资源！\n");
        }
      } else if (current_source_mode == 1) {
        if (cap.open(video_path)) {
          printf("🎬 系统已切换至：视频分析模式 (%s)\n", video_path.c_str());
        } else {
          printf("❌ [致命错误] 视频文件打开失败！OpenCV "
                 "缺少解码器或路径错误！\n");
          // 自动退回，防止后续拉取空帧
          current_source_mode = 0;
        }
      }
      source_changed = false;
    }

    // 【防呆与超时自动降级保护】
    if (!cap.isOpened()) {
      if (current_source_mode == 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_frame_time).count();
        if (elapsed >= 3) {
          printf("🚨 [Camera Fallback] 摄像头打开失败超时，自动降级切换至本地测试视频源...\n");
          video_path = "/home/user/test2.mp4";
          current_source_mode = 1;
          source_changed = true;
          last_frame_time = now;
          continue;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }

    cap >> tmp_frame;
    if (tmp_frame.empty()) {
      if (current_source_mode == 1) {
        printf("✅ 视频分析完毕，正在自动切回实时监控...\n");
        current_source_mode = 0;
        source_changed = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }
      
      // 摄像头拉流空帧超时判定
      if (current_source_mode == 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_frame_time).count();
        if (elapsed >= 3) {
          printf("🚨 [Camera Fallback] 视频流读取连续超时 3 秒，自动降级切换至本地测试视频源...\n");
          video_path = "/home/user/test2.mp4";
          current_source_mode = 1;
          source_changed = true;
          last_frame_time = now;
          continue;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // 成功获取有效帧，刷新最后帧时间
    last_frame_time = std::chrono::steady_clock::now();

    if (current_source_mode == 1) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(30)); // 导入视频时限速
    }

    // 实时性管理：基于无锁队列的旧帧淘汰机制
    // 注意：摄像头抓取的底层 buffer 会复用，所以入队时必须 clone()
    // 【Bug 修复】：消灭双重 clone，只拷贝一次
    cv::Mat push_frame = tmp_frame.clone();
    if (!cap_queue.push(push_frame)) {
      // 严格遵循 SPSC 无锁队列要求：满载时直接丢弃新帧，生产者绝对不能调用 pop()，否则会导致 Segmentation fault
    }
  }
  cap.release();
}
