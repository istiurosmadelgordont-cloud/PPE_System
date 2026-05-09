/**
 * @file      io_node.cpp
 * @brief     持久化数据存储与磁盘监控节点
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-16
 * @note      负责违规图片的异步落盘，以及磁盘使用率超限时的自动清理机制。
 */
#include "io_node.hpp"
#include "global_context.hpp"
#include "core_config.hpp"
#include <thread>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h> 
#include <filesystem>    
#include <ctime>
#include <fstream>  
#include <iostream>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

void check_and_cleanup_disk(const std::string& dir_path) {
    struct statvfs stat;
    if (statvfs(dir_path.c_str(), &stat) != 0) return;

    double total_bytes = (double)stat.f_blocks * stat.f_frsize;
    double available_bytes = (double)stat.f_bavail * stat.f_frsize;
    double used_ratio = (total_bytes - available_bytes) / total_bytes;

    if (used_ratio > 0.85) {
        printf("⚠️ [存储警告] 磁盘使用率达 %.1f%%，触发自我保护清理机制！\n", used_ratio * 100);

        std::vector<fs::path> jpg_files;
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.path().extension() == ".jpg") {
                jpg_files.push_back(entry.path());
            }
        }

        if (jpg_files.size() < 100) return;

        std::sort(jpg_files.begin(), jpg_files.end(), [](const fs::path& a, const fs::path& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });

        int delete_count = jpg_files.size() * 0.2;
        for (int i = 0; i < delete_count; ++i) {
            std::error_code ec;
            fs::remove(jpg_files[i], ec); 
        }

        printf("🧹 [存储清理] 危机解除！已自动删除 %d 张最旧的抓拍证据。\n", delete_count);
    }
}

void io_thread_func() {
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(CORE_IO, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    std::string base_dir = "/home/user/ppe/ppe_system/violations_data";
    mkdir(base_dir.c_str(), 0777); 

    std::string log_path = base_dir + "/violation_log.csv";
    std::ofstream log_file(log_path, std::ios::app);

    if (!log_file.is_open()) {
        printf("🚨 [IO 错误] 无法创建日志文件，请检查路径权限: %s\n", log_path.c_str());
    } else {
        printf("📁 [IO 系统] 绝对路径已激活: %s\n", base_dir.c_str());
    }

    int save_counter = 0; 

    while (is_running) {
        AlarmEvent event;
        if (alarm_queue.pop(event)) {
            auto now_c = std::chrono::system_clock::to_time_t(event.alarm_time);
            char time_str[64];
            std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", std::localtime(&now_c));

            std::string img_filename = base_dir + "/" + std::string(time_str) + "_" + event.violation_type + ".jpg";
            
            cv::imwrite(img_filename, event.frame);

            if (log_file.is_open()) {
                log_file << time_str << "," << event.violation_type << "," << img_filename << std::endl;
                log_file.flush(); 
            }

            printf("💾 [IO 写盘] 违规证据已保存至: %s\n", img_filename.c_str());

            // ==========================================
            // 【优化落实】：降低 IO 检查频率，杜绝系统大卡顿
            // ==========================================
            save_counter++;
            if (save_counter >= 500) {
                check_and_cleanup_disk(base_dir);
                save_counter = 0;
            }

        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (log_file.is_open()) log_file.close();
}
