/**
 * @file      rpmsg_node.cpp
 * @brief     底层硬件关联与 OpenAMP 跨核通信模块 (纯事件驱动版)
 * @author    [双生序章]
 * @version   3.0.0
 */
#include "rpmsg_node.hpp"
#include "ui_main_window.hpp" // 【绝对核心】：必须引入它，才能使用 SignalBridge 发送信号
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/rpmsg.h>
#include <poll.h>
#include <chrono>

#define MAX_DATA_LENGTH 256
#define DEVICE_CORE_BUZZER_CTRL 0x0005U
#define DEVICE_CORE_FIRE_REPORT 0x0006U

#pragma pack(push, 1)
typedef struct {
    uint32_t command;
    uint16_t length;
    char data[MAX_DATA_LENGTH];
} data_packet;
#pragma pack(pop)

RPMsgController::RPMsgController() : rpmsg_fd(-1), is_connected(false), is_buzzer_on(false) {}

RPMsgController::~RPMsgController() {
    cleanup();
}

bool RPMsgController::init() {
    std::lock_guard<std::mutex> lock(mtx);
    if (is_connected) return true;

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
        rpmsg_fd = open("/dev/rpmsg0", O_RDWR | O_NONBLOCK);
        if (rpmsg_fd > 0) {
            is_connected = true;
            printf("🔌 [RPMsg] 成功打通从核通信节点 /dev/rpmsg0！\n");
            
            // 启动底层监听线程
            rx_running = true;
            rx_thread = std::thread(&RPMsgController::rx_task, this);
        } else {
            printf("⚠️ [RPMsg] 打开端点 /dev/rpmsg0 失败！\n");
        }
    } else {
        printf("⚠️ [RPMsg] 创建 OpenAMP 端点失败！\n");
    }
    
    close(ctrl_fd);
    return is_connected;
}

void RPMsgController::set_buzzer(bool on) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!is_connected || rpmsg_fd < 0) return;
    if (is_buzzer_on == on) return;

    data_packet pkt;
    memset(&pkt, 0, sizeof(data_packet));
    pkt.command = DEVICE_CORE_BUZZER_CTRL;
    pkt.length = 1;
    pkt.data[0] = on ? '1' : '0';

    if (write(rpmsg_fd, &pkt, sizeof(data_packet)) > 0) {
        is_buzzer_on = on;
        printf(on ? "🔊 [物理警报] 已发送指令：蜂鸣器 开启\n" : "🔇 [物理警报] 已发送指令：蜂鸣器 关闭\n");
    }
}

void RPMsgController::cleanup() {
    rx_running = false;
    if (rx_thread.joinable()) {
        rx_thread.join();
    }

    std::lock_guard<std::mutex> lock(mtx);
    if (is_connected && rpmsg_fd > 0) {
        data_packet pkt;
        memset(&pkt, 0, sizeof(data_packet));
        pkt.command = DEVICE_CORE_BUZZER_CTRL;
        pkt.length = 1;
        pkt.data[0] = '0';
        write(rpmsg_fd, &pkt, sizeof(data_packet));
        
        close(rpmsg_fd);
        is_connected = false;
        rpmsg_fd = -1;
    }
}

// ==========================================
// 【架构变革】：纯粹的事件驱动发射源
// ==========================================
// ==========================================
// 【架构变革】：带有非对称防抖的事件驱动发射源
// ==========================================
// ==========================================
// 【架构终局】：时空解耦的非对称防抖状态机
// ==========================================
void RPMsgController::rx_task() {
    data_packet pkt;
    int safe_counter = 0;
    struct pollfd pfd;
    pfd.fd = rpmsg_fd;
    pfd.events = POLLIN;

    auto last_alarm_time = std::chrono::steady_clock::now();

    while (rx_running) {
        // ------------------------------------------------
        // 维度 1：空间（数据流）- 绝对敏锐的触发器
        // ------------------------------------------------
        if (rpmsg_fd > 0) {
        // 平时 200ms 唤醒一次，火警时 20ms 唤醒一次，将系统调用降低 90% 以上
        int timeout_ms = is_physical_alarm ? 20 : 200; 
        int ret = poll(&pfd, 1, timeout_ms);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            int n = read(rpmsg_fd, &pkt, sizeof(data_packet));
            // 只要读到有效数据，且是火警 '1'
            if (n > 0 && pkt.command == DEVICE_CORE_FIRE_REPORT && pkt.data[0] == '1') {
                // 瞬间续命：只要看到一丝火光，立刻清零安全倒计时！
                safe_counter = 0; 
                last_alarm_time = std::chrono::steady_clock::now();
                
                if (!is_physical_alarm) {
                    is_physical_alarm = true;
                    // 零延迟瞬间发报，UI 爆红
                    emit SignalBridge::getInstance()->sendPhysicalAlarmStatus(true);
                }
            }
        }

        // ------------------------------------------------
        // 维度 2：时间（时钟流）- 独立且强粘滞的解除器
        // ------------------------------------------------
        // 无论 read() 有没有读到数据，只要当前还在报警，时间就必须流逝！
        if (is_physical_alarm) {
            safe_counter++;
            
            // 2ms * 250 = 500 毫秒。
            // 意味着：哪怕底层断联、停发数据，只要 500 毫秒内没被 '1' 重新清零，系统就判定火源已彻底移除！
            if (safe_counter >= 250) {
            auto now = std::chrono::steady_clock::now();
            // 通过真实时间差校验，500毫秒内未收到火警信号，则解除报警
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_alarm_time).count() >= 500) {
                is_physical_alarm = false;
                // 彻底解除，UI 恢复绿色
                emit SignalBridge::getInstance()->sendPhysicalAlarmStatus(false);
            }
        }

        // 保持极低开销的 2ms 极限探雷频率
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}           
            // 2ms * 250 = 500 毫秒。
            // 意味着：哪怕底层断联、停发数据，只要 500 毫秒内没被 '1' 重新清零，系统就判定火源已彻底移除！
            if (safe_counter >= 250) {
            auto now = std::chrono::steady_clock::now();
            // 通过真实时间差校验，500毫秒内未收到火警信号，则解除报警
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_alarm_time).count() >= 500) {
                is_physical_alarm = false;
                // 彻底解除，UI 恢复绿色
                emit SignalBridge::getInstance()->sendPhysicalAlarmStatus(false);
            }
        }

        // 保持极低开销的 2ms 极限探雷频率
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}