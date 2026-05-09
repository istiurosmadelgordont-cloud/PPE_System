#ifndef RPMSG_NODE_HPP
#define RPMSG_NODE_HPP

#include <cstdint>
#include <mutex>
#include <atomic> // 新增
#include <thread> // 新增

// 采用单例模式管理底层唯一的物理通道
class RPMsgController {
public:
    // 获取全局唯一实例
    static RPMsgController& getInstance() {
        static RPMsgController instance;
        return instance;
    }

    // 删除拷贝构造和赋值操作，防止物理节点被重复打开
    RPMsgController(const RPMsgController&) = delete;
    RPMsgController& operator=(const RPMsgController&) = delete;

    // 初始化硬件连接
    bool init();

    // 控制蜂鸣器 (true: 响, false: 停)
    void set_buzzer(bool on);

    // 安全释放资源
    void cleanup();

    std::atomic<bool> is_physical_alarm{false};

private:
    RPMsgController();
    ~RPMsgController();

    int rpmsg_fd;
    bool is_connected;
    bool is_buzzer_on; // 状态记忆，防止重复下发相同指令
    std::mutex mtx;    // 线程安全锁
    std::thread rx_thread;
    std::atomic<bool> rx_running{false};
    void rx_task();
};

#endif // RPMSG_NODE_HPP