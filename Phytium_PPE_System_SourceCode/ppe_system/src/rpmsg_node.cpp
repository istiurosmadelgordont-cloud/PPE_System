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
#include "core_config.hpp"
#include <chrono>
#include <fcntl.h>
#include <linux/rpmsg.h>
#include <linux/gpio.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <glob.h>
#include <vector>
#include <algorithm>
#include <string>

#define MAX_DATA_LENGTH 255             ///< 最大数据包长度 (255 数据 + 1 校验 = 256 字节，总包 262 字节)
#define DEVICE_CORE_BUZZER_CTRL 0x0005U ///< 蜂鸣器控制命令字

// 极速拉高/拉低 GPIO4_13 (gpiochip4 的第 13 号 line)
static void write_gpio_gpiod(int value) {
  static int line_fd = -1;
  if (line_fd < 0) {
    int chip_fd = open("/dev/gpiochip4", O_RDWR);
    if (chip_fd >= 0) {
      struct gpiohandle_request req;
      memset(&req, 0, sizeof(req));
      req.lineoffsets[0] = 13; // GPIO4_13
      req.flags = GPIOHANDLE_REQUEST_OUTPUT;
      req.lines = 1;
      req.default_values[0] = 1; // 默认拉高电平 (1)
      strncpy(req.consumer_label, "latency_test", sizeof(req.consumer_label));
      if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) >= 0) {
        line_fd = req.fd;
      }
      close(chip_fd);
    }
  }
  if (line_fd >= 0) {
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    data.values[0] = value;
    ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
  }
}
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
  uint8_t crc8;                     // 追加 CRC-8 校验字段
} data_packet;
#pragma pack(pop)

static bool is_slave_core_running() {
  FILE* fp = fopen("/sys/class/remoteproc/remoteproc0/state", "r");
  if (!fp) return false;
  char buf[32];
  memset(buf, 0, sizeof(buf));
  if (fgets(buf, sizeof(buf) - 1, fp) != nullptr) {
    char* newline = strchr(buf, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buf, '\r');
    if (newline) *newline = '\0';
  }
  fclose(fp);
  return strcmp(buf, "running") == 0;
}

static std::string find_active_rpmsg_device() {
  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));
  // 匹配所有 /dev/rpmsg[0-9]*，排除 /dev/rpmsg_ctrl0
  int return_value = glob("/dev/rpmsg[0-9]*", 0, NULL, &glob_result);
  if (return_value != 0) {
    globfree(&glob_result);
    return "/dev/rpmsg0"; // 默认回退
  }
  
  std::vector<std::string> devices;
  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    devices.push_back(glob_result.gl_pathv[i]);
  }
  globfree(&glob_result);
  
  if (devices.empty()) {
    return "/dev/rpmsg0";
  }
  
  // 提取后缀数字并排序，选择数字最大的那一个（即最新注册的活动通信信道）
  std::sort(devices.begin(), devices.end(), [](const std::string& a, const std::string& b) {
    int num_a = -1;
    int num_b = -1;
    sscanf(a.c_str(), "/dev/rpmsg%d", &num_a);
    sscanf(b.c_str(), "/dev/rpmsg%d", &num_b);
    return num_a < num_b;
  });
  
  printf("🔎 [RPMsg] 发现多个端点，自动选择当前活动节点: %s\n", devices.back().c_str());
  fflush(stdout);
  return devices.back();
}

RPMsgController::RPMsgController()
    : rpmsg_fd(-1), is_connected(false), is_buzzer_on(false), heartbeat_running(false) {}

RPMsgController::~RPMsgController() { cleanup(); }

bool RPMsgController::init() {
  std::lock_guard<std::mutex> lock(mtx);
  if (is_connected)
    return true;

  if (!is_slave_core_running()) {
    printf("⚠️ [RPMsg] 从核未启动，暂不打通 /dev/rpmsg0，开启接收与心跳守护线程，等待自动重连...\n");
    fflush(stdout);
    rx_running = true;
    rx_thread = std::thread(&RPMsgController::rx_task, this);
    heartbeat_running = true;
    heartbeat_thread = std::thread(&RPMsgController::heartbeat_task, this);
    return false;
  }

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
    std::string active_dev = find_active_rpmsg_device();
    for (int retry = 0; retry < 10; ++retry) {
      rpmsg_fd = open(active_dev.c_str(), O_RDWR | O_NONBLOCK);
      if (rpmsg_fd > 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (rpmsg_fd > 0) {
      is_connected = true;
      printf("🔌 [RPMsg] 成功打通从核通信节点 %s！\n", active_dev.c_str());

      // 默认拉高测试引脚 GPIO4_13 (1)
      write_gpio_gpiod(1);

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

  // 触发违规时拉低 GPIO4_13 (0)，恢复时拉高 (1)
  write_gpio_gpiod(on ? 0 : 1);

  data_packet pkt;
  memset(&pkt, 0, sizeof(data_packet));
  pkt.command = DEVICE_CORE_BUZZER_CTRL;
  pkt.length = 1;
  pkt.data[0] = on ? '1' : '0';
  pkt.crc8 = calculate_crc8((const uint8_t *)&pkt, sizeof(data_packet) - 1);

  int n = write(rpmsg_fd, &pkt, sizeof(data_packet));
  if (n > 0) {
    is_buzzer_on = on;
    printf(on ? "🔊 [物理警报] 已发送指令：蜂鸣器 开启\n"
              : "🔇 [物理警报] 已发送指令：蜂鸣器 关闭\n");
    fflush(stdout);
  } else {
    printf("🚨 [RPMsg TX] 蜂鸣器控制包发送失败 (errno=%d, %s)，连接断开！\n", errno, strerror(errno));
    fflush(stdout);
    is_connected = false;
    int fd_to_close = rpmsg_fd.exchange(-1);
    if (fd_to_close > 0) {
      close(fd_to_close);
    }
  }
}

void RPMsgController::trigger_crc_test() {
  for (int i = 0; i < 10; i++) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (is_connected && rpmsg_fd >= 0) {
        data_packet pkt;
        memset(&pkt, 0, sizeof(data_packet));
        pkt.command = 0x0099U; // DEVICE_CORE_CRC_TEST
        pkt.length = 0;
        pkt.crc8 = calculate_crc8((const uint8_t *)&pkt, sizeof(data_packet) - 1);

        int n = write(rpmsg_fd, &pkt, sizeof(data_packet));
        if (n > 0) {
          printf("🧪 [测试] 已向从核发送 CRC 校验测试触发指令\n");
          fflush(stdout);
          return;
        } else {
          is_connected = false;
          int fd_to_close = rpmsg_fd.exchange(-1);
          if (fd_to_close > 0) {
            close(fd_to_close);
          }
          return;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  printf("❌ [测试] 发送 CRC 校验测试触发指令失败：连接超时\n");
  fflush(stdout);
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
    // 退出时恢复拉高测试引脚 GPIO4_13 (1)
    write_gpio_gpiod(1);
    // 1. 关闭从核报警蜂鸣器
    data_packet pkt;
    memset(&pkt, 0, sizeof(data_packet));
    pkt.command = DEVICE_CORE_BUZZER_CTRL;
    pkt.length = 1;
    pkt.data[0] = '0';
    pkt.crc8 = calculate_crc8((const uint8_t *)&pkt, sizeof(data_packet) - 1);
    write(rpmsg_fd, &pkt, sizeof(data_packet));

    // 2. 发送从核安全关闭命令，停止从核看门狗以防意外整机重启
    printf("🔌 [RPMsg] 正在向从核发送安全关闭命令...\n");
    data_packet shut_pkt;
    memset(&shut_pkt, 0, sizeof(data_packet));
    shut_pkt.command = DEVICE_CORE_SHUTDOWN;
    shut_pkt.length = 0;
    shut_pkt.crc8 = calculate_crc8((const uint8_t *)&shut_pkt, sizeof(data_packet) - 1);
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

  int last_emitted_fire_status = -1;
  int last_emitted_gas_status = -1;

  while (rx_running) {
    if (rpmsg_fd < 0) {
      // 如果从核未处于 running 状态，跳过端点创建，以防 ioctl 强行通信引发内核死锁/挂起
      if (!is_slave_core_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }

      // 【自动重连】当设备节点不存在时，循环尝试创建并重新打开
      int ctrl_fd = open("/dev/rpmsg_ctrl0", O_RDWR);
      if (ctrl_fd >= 0) {
        struct rpmsg_endpoint_info eptinfo;
        memset(&eptinfo, 0, sizeof(eptinfo));
        strncpy(eptinfo.name, "rpmsg-openamp-demo-channel", sizeof(eptinfo.name));
        eptinfo.src = 0;
        eptinfo.dst = 0;
        ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &eptinfo);
        close(ctrl_fd);
      }
      
      std::string active_dev = find_active_rpmsg_device();
      int new_fd = open(active_dev.c_str(), O_RDWR | O_NONBLOCK);
      if (new_fd > 0) {
        rpmsg_fd = new_fd;
        is_connected = true;
        pfd.fd = new_fd;
        pfd.events = POLLIN;
        printf("🔌 [RPMsg RX] 成功重新打通从核通信节点 %s！\n", active_dev.c_str());
        fflush(stdout);
      } else {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 1秒后重试
      }
      continue;
    }

    // 平时 200ms 轮询一次，报警中 50ms 轮询一次
    int timeout_ms = is_physical_alarm ? 50 : 200;
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
      printf("🚨 [RPMsg RX] poll检测到端点异常 (HUP/ERR)，连接断开！\n");
      fflush(stdout);
      is_connected = false;
      int fd_to_close = rpmsg_fd.exchange(-1);
      if (fd_to_close > 0) {
        close(fd_to_close);
      }
      continue;
    }

    if (ret > 0 && (pfd.revents & POLLIN)) {
      int n = read(rpmsg_fd, &pkt, sizeof(data_packet));

      if (n <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          printf("🚨 [RPMsg RX] 读取失败 (n=%d, errno=%d)，连接断开！\n", n, errno);
          fflush(stdout);
          is_connected = false;
          int fd_to_close = rpmsg_fd.exchange(-1);
          if (fd_to_close > 0) {
            close(fd_to_close);
          }
        }
        continue;
      }
        if (n < 7) {
          continue;
        }
        uint8_t received_crc = ((uint8_t *)&pkt)[n - 1];
        uint8_t calculated_crc = calculate_crc8((const uint8_t *)&pkt, n - 1);
        if (received_crc != calculated_crc) {
          static int crc_err_count = 0;
          crc_err_count++;
          printf("⚠️ [CRC] 数据校验失败 #%d，丢弃该包\n", crc_err_count);
          fflush(stdout);
          emit SignalBridge::getInstance()->sendCrcError(crc_err_count);
          continue;
        }

        printf("[RPMsg RX] 收到数据包: 命令=0x%x, 长度=%d\n", pkt.command, pkt.length);
        fflush(stdout);
        if (pkt.command == DEVICE_CORE_FIRE_REPORT) {
          char val = pkt.data[0];
          int current_status = 0;
          if (val == '2') {
            is_physical_alarm = false;
            current_status = 2;
          } else if (val == '1') {
            auto now = std::chrono::steady_clock::now();

            // 如果已经在报警中，只需要续命（刷新最后一次火警时间）
            if (is_physical_alarm) {
              last_fire_time = now;
              current_status = 1;
            } else {
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
                current_status = 1;
              } else {
                printf("🔸 [火警预警] 收到第 %d/%d 次信号，等待确认...\n", fire_count,
                       CONFIRM_COUNT);
                current_status = last_emitted_fire_status;
              }
            }
          } else {
            if (is_physical_alarm) {
              current_status = 1;
            } else {
              current_status = 0;
            }
          }

          if (current_status != last_emitted_fire_status && current_status != -1) {
            last_emitted_fire_status = current_status;
            emit SignalBridge::getInstance()->sendPhysicalAlarmStatus(current_status);
          }
        }
        else if (pkt.command == DEVICE_CORE_GAS_REPORT) {
          printf("☁️ [RPMsg RX] 收到可燃气体上报: 状态值='%c' (0:清洁, 1:超标报警, 2:断开)\n", pkt.data[0]);
          fflush(stdout);
          char val = pkt.data[0];
          int current_status = 0;
          if (val == '2') {
            is_gas_alarm = false;
            current_status = 2;
          } else if (val == '1') {
            auto now = std::chrono::steady_clock::now();
            if (is_gas_alarm) {
              last_gas_time = now;
              current_status = 1;
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
                current_status = 1;
              } else {
                printf("☁️ [气体警报预警] 收到第 %d/%d 次信号，等待确认...\n", gas_count, GAS_CONFIRM_COUNT);
                fflush(stdout);
                current_status = last_emitted_gas_status;
              }
            }
          } else {
            if (is_gas_alarm) {
              current_status = 1;
            } else {
              current_status = 0;
            }
          }

          if (current_status != last_emitted_gas_status && current_status != -1) {
            last_emitted_gas_status = current_status;
            emit SignalBridge::getInstance()->sendGasAlarmStatus(current_status);
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
        else if (pkt.command == DEVICE_CORE_CHECK) {
          if (pkt.length == sizeof(int64_t)) {
            int64_t send_time_ns;
            memcpy(&send_time_ns, pkt.data, sizeof(int64_t));
            int64_t recv_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
            double rtt_ms = (recv_time_ns - send_time_ns) / 1000000.0;
            emit SignalBridge::getInstance()->sendSlaveLatency(rtt_ms);
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
        if (last_emitted_fire_status != 0) {
          last_emitted_fire_status = 0;
          emit SignalBridge::getInstance() -> sendPhysicalAlarmStatus(0);
        }
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
        if (last_emitted_gas_status != 0) {
          last_emitted_gas_status = 0;
          emit SignalBridge::getInstance()->sendGasAlarmStatus(0);
        }
      }
    }
  }
}

void RPMsgController::heartbeat_task() {
  data_packet pkt;
  memset(&pkt, 0, sizeof(data_packet));
  pkt.command = DEVICE_CORE_CHECK;

  auto last_send_time = std::chrono::steady_clock::now();

  while (heartbeat_running) {
    // 每 500ms 发送一次心跳包
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 【主核侧失联检测】检查本次唤醒距离上次发送的间隔
    auto now = std::chrono::steady_clock::now();
    auto gap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count();
    if (gap_ms > 2000) {
      printf("🚨🚨🚨 [主核心跳检测] 检测到心跳中断！间隔=%lldms (正常应≤600ms)，进程可能被挂起过！\n", (long long)gap_ms);
      printf("🚨🚨🚨 [主核心跳检测] 从核将在检测到心跳丢失后触发看门狗自愈重启！\n");
      fflush(stdout);
    }
    last_send_time = now;

    std::lock_guard<std::mutex> lock(mtx);
    if (is_connected && rpmsg_fd > 0) {
      int64_t send_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
      memcpy(pkt.data, &send_time_ns, sizeof(int64_t));
      pkt.length = sizeof(int64_t);
      pkt.crc8 = calculate_crc8((const uint8_t *)&pkt, sizeof(data_packet) - 1);
      int n = write(rpmsg_fd, &pkt, sizeof(data_packet));
      if (n < 0) {
        printf("🚨 [RPMsg TX] 心跳包发送失败 (errno=%d, %s)，连接断开！\n", errno, strerror(errno));
        fflush(stdout);
        is_connected = false;
        int fd_to_close = rpmsg_fd.exchange(-1);
        if (fd_to_close > 0) {
          close(fd_to_close);
        }
      }
    }
  }
}