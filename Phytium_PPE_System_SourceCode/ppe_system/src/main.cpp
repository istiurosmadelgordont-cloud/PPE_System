/**
 * @file      main.cpp
 * @brief     系统总调度入口与资源初始化 (System Entry Point)
 * @details   负责 Qt
 * 引擎启动、底层驱动握手及四大核心（UI、摄像头、AI推理、IO调度）并行线程的分发绑核。
 *            基于 Phytium Pi (E2000Q)
 * 的四核异构架构，实现任务的空间隔离与极致性能。
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */
#include "core_config.hpp"
#include "global_context.hpp"
#include "rpmsg_node.hpp"
#include "ui_main_window.hpp"
#include <QApplication>
#include <thread>

// 全局变量已统一在 global_context.hpp 中声明和定义

// 声明后台线程函数
extern void camera_thread_func();
extern void inference_thread_func();
extern void io_thread_func();

int main(int argc, char *argv[]) {
  // 1. 初始化图形引擎 (GUI Engine)
  QApplication app(argc, argv);

  // [新增] 提前拉起异构通信模块
  RPMsgController::getInstance().init();

  // 2. 将主线程（包含 Qt 的事件渲染循环）绑定在 Core 0 (小核)
  // 保证图形界面的流畅性，不与高计算密度的 AI 线程抢占资源
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(CORE_IO, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  // 3.启动所有异步后台线程
  std::thread t_cam(camera_thread_func);
  std::thread t_ai(inference_thread_func);
  std::thread t_io(io_thread_func);

  t_cam.detach();
  t_ai.detach();
  t_io.detach();

  // 4.显示控制台界面
  MainWindow window;
  window.showFullScreen();

  // 5. Core 0 在此处陷入死循环，专注响应 UI 事件驱动与图表重绘
  return app.exec();
}
