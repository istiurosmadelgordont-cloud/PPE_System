/**
 * @file      main.cpp
 * @brief     系统总调度入口与资源初始化
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-16
 * @note      负责 Qt 引擎启动、底层驱动握手及四大核心并行线程的分发绑核。
 */
#include <QApplication>
#include "global_context.hpp"
#include "ui_main_window.hpp"
#include "core_config.hpp"
#include "rpmsg_node.hpp"
#include <thread>

//全局控制
//bool is_running = true;

//声明后台线程函数
extern void camera_thread_func();
extern void inference_thread_func();
extern void io_thread_func();

int main(int argc, char *argv[]) {
    //1.初始化图形引擎
    QApplication app(argc, argv);
    
    // [新增] 提前拉起异构通信模块
    RPMsgController::getInstance().init();

    //2.将主线程包含Qt的事件渲染循环绑定在0号小核
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(CORE_IO, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    //3.启动所有异步后台线程
    std::thread t_cam(camera_thread_func);
    std::thread t_ai(inference_thread_func);
    std::thread t_io(io_thread_func);
    
    t_cam.detach(); 
    t_ai.detach(); 
    t_io.detach();

    //4.显示控制台界面
    MainWindow window;
    window.show();
    
    //5.0号核在此处陷入死循环，疯狂响应按钮点击和画图请求
    return app.exec(); 
}
