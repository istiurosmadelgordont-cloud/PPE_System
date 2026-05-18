/**
 * @file      lockfree_queue.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once
#include <atomic>
#include <cstddef>

template <typename T, size_t Size> class LockFreeRingBuffer {
private:
  T buffer[Size];
  // 【优化 5】：cache line padding 防止伪共享
  // 飞腾 E2000Q 的 cache line 为 64 字节，head 和 tail 分别被
  // 生产者和消费者高频访问，必须隔离在不同的 cache line 上
  alignas(64) std::atomic<size_t> head{0};
  alignas(64) std::atomic<size_t> tail{0};

public:
  // 生产者调用：非阻塞推入数据
  bool push(const T &item) {
    size_t current_tail = tail.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % Size;
    if (next_tail == head.load(std::memory_order_acquire)) {
      // 【防 OOM 机制】：队列满了直接返回 false，把新数据丢弃，绝不卡死等待
      return false;
    }
    buffer[current_tail] = item;
    tail.store(next_tail, std::memory_order_release);
    return true;
  }

  // 消费者调用：非阻塞取出数据
  bool pop(T &item) {
    size_t current_head = head.load(std::memory_order_relaxed);
    if (current_head == tail.load(std::memory_order_acquire)) {
      // 队列为空，直接返回 false，让消费者去干别的
      return false;
    }
    item = buffer[current_head];
    head.store((current_head + 1) % Size, std::memory_order_release);
    return true;
  }
};