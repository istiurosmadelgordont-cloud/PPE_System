/**
 * @file      rpmsg_node.hpp
 * @brief     底层异构通信节点 (OpenAMP RPMsg Controller)
 * @details   采用单例模式封装，负责飞腾派主核 (Core 0) 与从核 (Core 1)
 * 之间的低延迟异步双向通信。
 *            主要用于接收底层物理传感器（如火焰探头）的报警中断，并控制蜂鸣器等外设硬件。
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#ifndef RPMSG_NODE_HPP
#define RPMSG_NODE_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

/**
 * @class RPMsgController
 * @brief RPMsg 跨核通信控制器（单例）
 *
 * 管理 /dev/rpmsg0
 * 字符设备，通过轮询与非对称滞回状态机，提供高可靠的硬件事件监听与命令下发能力。
 */
class RPMsgController {
public:
  /**
   * @brief  获取通信控制器的全局唯一实例 (单例模式)
   * @return RPMsgController& 实例引用
   */
  static RPMsgController &getInstance() {
    static RPMsgController instance;
    return instance;
  }

  // 删除拷贝构造函数与赋值运算符，严格保证物理通信通道的唯一性
  RPMsgController(const RPMsgController &) = delete;
  RPMsgController &operator=(const RPMsgController &) = delete;

  /**
   * @brief  初始化底层硬件连接
   * @details 打开控制端点，建立与 OpenAMP 从核的通道，并拉起后台监听线程。
   * @return bool 初始化成功返回 true，否则返回 false
   */
  bool init();

  /**
   * @brief  控制物理蜂鸣器状态
   * @param  on true 为开启警报响铃，false 为关闭
   */
  void set_buzzer(bool on);

  /**
   * @brief  安全释放硬件资源并销毁监听线程
   */
  void cleanup();

  /**
   * @brief  获取当前通道连接状态
   */
  bool isConnected() const { return is_connected; }

  /// @brief 全局原子状态标识，指示当前是否处于物理火警触发状态
  std::atomic<bool> is_physical_alarm{false};

private:
  /**
   * @brief 私有构造与析构函数
   */
  RPMsgController();
  ~RPMsgController();

  int rpmsg_fd;                 ///< 字符设备文件描述符
  bool is_connected;            ///< 物理通道连接状态
  bool is_buzzer_on;            ///< 蜂鸣器状态记忆（防止重复下发冗余指令）
  std::mutex mtx;               ///< 线程安全互斥锁，保护写操作
  std::thread rx_thread;        ///< 异步数据接收线程
  std::atomic<bool> rx_running; ///< 接收线程运行标识
  std::thread heartbeat_thread;   ///< 定时心跳发送线程
  std::atomic<bool> heartbeat_running; ///< 心跳线程运行状态

  /**
   * @brief  异步接收任务流
   * @details 内部实现非对称滞回防抖状态机，过滤传感器毛刺与电磁干扰。
   */
  void rx_task();

  /**
   * @brief  心跳任务，定期发送心跳帧以刷新OpenAMP共享内存队列
   */
  void heartbeat_task();
};

#endif // RPMSG_NODE_HPP