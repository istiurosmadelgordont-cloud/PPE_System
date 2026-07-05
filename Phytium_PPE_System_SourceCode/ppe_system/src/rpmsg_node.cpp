/**
 * @file      rpmsg_node.cpp
 * @brief     底层异构通信节点实现 (OpenAMP RPMsg Controller)
 * @details   实现与飞腾派从核的 /dev/rpmsg0 通信，包含非对称防抖机制。
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#include "rpmsg_node.hpp"
#include "ui_main_window.hpp" // 【绝对核心】：必须引入它，才能使用 SignalBridge 发送信号
#include <chrono>
#include <fcntl.h>
#include <linux/rpmsg.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#define MAX_DATA_LENGTH 256             ///< 最大数据包长度
#define DEVICE_CORE_BUZZER_CTRL 0x0005U ///< 蜂鸣器控制命令字
#define DEVICE_CORE_FIRE_REPORT 0x0006U ///< 火焰探头报警命令字
#define DEVICE_CORE_GAS_REPORT 0x0007U  ///< 气体报警命令字
#define DEVICE_CORE_ENV_REPORT 0x0008U  ///< 温湿度数据命令字
#define DEVICE_CORE_CHECK 0x0003U       ///< 心跳及主动刷新命令字
#define DEVICE_CORE_SHUTDOWN 0x0002U    ///< 从核安全停止并关闭看门狗命令字

#pragma pack(push, 1)
typedef struct {
  uint32_t command;
  uint16_t length;
  char data[MAX_DATA_LENGTH];
} data_packet;
#pragma pack(pop)

RPMsgController::RPMsgController()
    : rpmsg_fd(-1), is_connected(false), is_buzzer_on(false), heartbeat_running(false) {}

RPMsgController::~RPMsgController() { cleanup(); }

bool RPMsgController::init() {
  std::lock_guard<std::mutex> lock(mtx);
  if (is_connected)
    return true;

  int ctrl_fd = open("/dev/rpmsg_ctrl0", O_RDWR);
  if (ctrl_fd < 0) {
    printf("⚠️ [RPMsg] 无法打开 rpmsg_ctrl0，硬件联动模块初始化失败！\n");
    return false;
  }

  struct rpmsg_endpoint_info eptinfo;
  memset(&eptinfo, 0, sizeof(eptinfo));
  strncpy(eptinfo.name, "rpmsg-openamp-demo-channel", sizeof(eptinfo.name));
  eptinfo.src = 0;
  eptinfo.dst = 0;

  if (ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &eptinfo) == 0) {
    // 带有重试和微延迟的打开逻辑，防止 udev 权限异步修改的竞争风险
    for (int retry = 0; retry < 10; ++retry) {
      rpmsg_fd = open("/dev/rpmsg0", O_RDWR | O_NONBLOCK);
      if (rpmsg_fd > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (rpmsg_fd > 0) {
      is_connected = true;
      printf("🔌 [RPMsg] 成功打通从核通信节点 /dev/rpmsg0！\n");

      // 启动底层监听线程
      rx_running = true;
      rx_thread = std::thread(&RPMsgController::rx_task, this);

      // 启动心跳及主动刷新线程
      heartbeat_running = true;
      heartbeat_thread = std::thread(&RPMsgController::heartbeat_task, this);
    } else {
      printf("⚠️ [RPMsg] 打开端点 /dev/rpmsg0 失败！(errno=%d, %s)\n", errno, strerror(errno));
    }
  } else {
    printf("⚠️ [RPMsg] 创建 OpenAMP 端点失败！(errno=%d, %s)\n", errno, strerror(errno));
  }

  close(ctrl_fd);
  return is_connected;
}

void RPMsgController::set_buzzer(bool on) {
  std::lock_guard<std::mutex> lock(mtx);
  if (!is_connected || rpmsg_fd < 0)
    return;
  if (is_buzzer_on == on)
    return;

  data_packet pkt;
  memset(&pkt, 0, sizeof(data_packet));
  pkt.command = DEVICE_CORE_BUZZER_CTRL;
  pkt.length = 1;
  pkt.data[0] = on ? '1' : '0';

  if (write(rpmsg_fd, &pkt, sizeof(data_packet)) > 0) {
    is_buzzer_on = on;
    printf(on ? "🔊 [物理警报] 已发送指令：蜂鸣器 开启\n"
              : "🔇 [物理警报] 已发送指令：蜂鸣器 关闭\n");
  }
}

void RPMsgController::cleanup() {
  heartbeat_running = false;
  if (heartbeat_thread.joinable()) {
    heartbeat_thread.join();
  }

  rx_running = false;
  if (rx_thread.joinable()) {
    rx_thread.join();
  }

  std::lock_guard<std::mutex> lock(mtx);
  if (is_connected && rpmsg_fd > 0) {
    // 1. 关闭从核报警蜂鸣器
    data_packet pkt;
    memset(&pkt, 0, sizeof(data_packet));
    pkt.command = DEVICE_CORE_BUZZER_CTRL;
    pkt.length = 1;
    pkt.data[0] = '0';
    write(rpmsg_fd, &pkt, sizeof(data_packet));

    // 2. 发送从核安全关闭命令，停止从核看门狗以防意外整机重启
    printf("🔌 [RPMsg] 正在向从核发送安全关闭命令...\n");
    data_packet shut_pkt;
    memset(&shut_pkt, 0, sizeof(data_packet));
    shut_pkt.command = DEVICE_CORE_SHUTDOWN;
    shut_pkt.length = 0;
    write(rpmsg_fd, &shut_pkt, sizeof(data_packet));
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 微小等待确保发送

    close(rpmsg_fd);
    is_connected = false;
    rpmsg_fd = -1;
  }
}

// ==========================================
// 【架构终局】：工业级非对称滞回防抖状态机
// ==========================================
// 设计思想：
//   触发要"慢"：连续收到 N 次火警才确认（防止电磁干扰误报）
//   解除要"更慢"：必须沉默 T 秒才解除（防止信号间隙导致红绿闪烁）
// ==========================================
void RPMsgController::rx_task() {
  data_packet pkt;
  struct pollfd pfd;
  pfd.fd = rpmsg_fd;
  pfd.events = POLLIN;

  // --- 防抖核心参数 ---
  constexpr int CONFIRM_COUNT = 1; // 触发阈值：收到 1 次火警信号就立刻确认并红灯
  constexpr int RELEASE_TIMEOUT_MS = 3000; // 解除阈值：沉默 3 秒才解除报警
  constexpr int CONFIRM_WINDOW_MS = 2000; // 确认窗口：3 次信号必须在 2 秒内完成

  int fire_count = 0;                                      // 累计收到的火警计数
  auto first_fire_time = std::chrono::steady_clock::now(); // 第一次火警的时间
  auto last_fire_time = std::chrono::steady_clock::now();  // 最近一次火警的时间

  // --- 可燃气体防抖核心参数 ---
  constexpr int GAS_CONFIRM_COUNT = 1; // 连续 1 次就立刻确认
  constexpr int GAS_RELEASE_TIMEOUT_MS = 3000; // 沉默 3 秒解除
  constexpr int GAS_CONFIRM_WINDOW_MS = 2000; // 2 秒确认窗口
  int gas_count = 0;
  auto first_gas_time = std::chrono::steady_clock::now();
  auto last_gas_time = std::chrono::steady_clock::now();
  bool is_gas_alarm = false;

  while (rx_running) {
    if (rpmsg_fd < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }

    // 平时 200ms 轮询一次，报警中 50ms 轮询一次
    int timeout_ms = is_physical_alarm ? 50 : 200;
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret > 0 && (pfd.revents & POLLIN)) {
      int n = read(rpmsg_fd, &pkt, sizeof(data_packet));

      if (n > 0) {
        printf("[RPMsg RX] 收到数据包: 命令=0x%x, 长度=%d\n", pkt.command, pkt.length);
        fflush(stdout);
        if (pkt.command == DEVICE_CORE_FIRE_REPORT && pkt.data[0] == '1') {
          auto now = std::chrono::steady_clock::now();

          // 如果已经在报警中，只需要续命（刷新最后一次火警时间）
          if (is_physical_alarm) {
            last_fire_time = now;
            continue;
          }

          // --- 尚未报警：执行计数确认逻辑 ---
          // 如果距离第一次计数已经超过确认窗口，重新开始计数
          auto elapsed_since_first =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - first_fire_time)
                  .count();

          if (fire_count == 0 || elapsed_since_first > CONFIRM_WINDOW_MS) {
            // 重置计数器，从头开始
            fire_count = 1;
            first_fire_time = now;
          } else {
            fire_count++;
          }

          last_fire_time = now;

          // 达到阈值 → 确认火警！
          if (fire_count >= CONFIRM_COUNT) {
            is_physical_alarm = true;
            fire_count = 0; // 重置计数器，为下次做好准备
            printf("🔥 [火警确认] 连续 %d 次信号确认，拉响警报！\n",
                   CONFIRM_COUNT);
            emit SignalBridge::getInstance() -> sendPhysicalAlarmStatus(true);
          } else {
            printf("🔸 [火警预警] 收到第 %d/%d 次信号，等待确认...\n", fire_count,
                   CONFIRM_COUNT);
          }
        }
        else if (pkt.command == DEVICE_CORE_GAS_REPORT) {
          bool gas_alarm_raw = (pkt.data[0] == '1');
          auto now = std::chrono::steady_clock::now();
          if (gas_alarm_raw) {
            if (is_gas_alarm) {
              last_gas_time = now;
            } else {
              auto elapsed_since_first =
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - first_gas_time)
                      .count();
              if (gas_count == 0 || elapsed_since_first > GAS_CONFIRM_WINDOW_MS) {
                gas_count = 1;
                first_gas_time = now;
              } else {
                gas_count++;
              }
              last_gas_time = now;
              if (gas_count >= GAS_CONFIRM_COUNT) {
                is_gas_alarm = true;
                gas_count = 0;
                printf("☁️ [气体警报确认] 连续 %d 次信号确认，拉响警报！\n", GAS_CONFIRM_COUNT);
                fflush(stdout);
                emit SignalBridge::getInstance()->sendGasAlarmStatus(true);
              } else {
                printf("☁️ [气体警报预警] 收到第 %d/%d 次信号，等待确认...\n", gas_count, GAS_CONFIRM_COUNT);
                fflush(stdout);
              }
            }
          }
        }
        else if (pkt.command == DEVICE_CORE_ENV_REPORT) {
          if (pkt.length < MAX_DATA_LENGTH) {
            pkt.data[pkt.length] = '\0';
          } else {
            pkt.data[MAX_DATA_LENGTH - 1] = '\0';
          }
          if (strcmp(pkt.data, "ERR") == 0) {
            printf("🌡️ [RPMsg] 警告：收到从温湿度传感器掉线上报！\n");
            fflush(stdout);
            emit SignalBridge::getInstance()->sendEnvError();
          } else {
            float temp = 0.0f, humid = 0.0f;
            if (sscanf(pkt.data, "T:%f,H:%f", &temp, &humid) == 2) {
              printf("🌡️ [RPMsg] 收到温湿度上报: T=%.1f C, H=%.1f %%\n", temp, humid);
              fflush(stdout);
              emit SignalBridge::getInstance()->sendEnvMetrics(temp, humid);
            }
          }
        }
      }
    }

    // --- 解除逻辑：必须沉默足够长时间才能解除 ---
    if (is_physical_alarm) {
      auto now = std::chrono::steady_clock::now();
      auto silence_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_fire_time)
                            .count();

      if (silence_ms >= RELEASE_TIMEOUT_MS) {
        is_physical_alarm = false;
        printf("✅ [火警解除] 已沉默 %dms，恢复正常状态\n", RELEASE_TIMEOUT_MS);
        emit SignalBridge::getInstance() -> sendPhysicalAlarmStatus(false);
      }
    }

    // --- 气体解除逻辑：必须沉默足够长时间才能解除 ---
    if (is_gas_alarm) {
      auto now = std::chrono::steady_clock::now();
      auto silence_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_gas_time)
                            .count();
      if (silence_ms >= GAS_RELEASE_TIMEOUT_MS) {
        is_gas_alarm = false;
        printf("✅ [气体警报解除] 已沉默 %dms，恢复正常状态\n", GAS_RELEASE_TIMEOUT_MS);
        fflush(stdout);
        emit SignalBridge::getInstance()->sendGasAlarmStatus(false);
      }
    }
  }
}

void RPMsgController::heartbeat_task() {
  data_packet pkt;
  memset(&pkt, 0, sizeof(data_packet));
  pkt.command = DEVICE_CORE_CHECK;
  pkt.length = 0;

  while (heartbeat_running) {
    // 每 500ms 发送一次空心跳包
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::lock_guard<std::mutex> lock(mtx);
    if (is_connected && rpmsg_fd > 0) {
      // 写入 6 字节数据头（4字节命令 + 2字节长度）
      // 写入操作会强制 Linux 内核驱动遍历并处理 TX 和 RX 描述符，从而拉取滞留的温湿度数据包
      int n = write(rpmsg_fd, &pkt, 6);
      (void)n;
    }
  }
}