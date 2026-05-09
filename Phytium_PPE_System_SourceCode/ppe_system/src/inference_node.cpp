/**
 * @file      inference_node.cpp
 * @brief     异构视觉推理大脑与 ByteTrack 追踪引擎
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-16
 * @note      物理绑核于 Core 2 & 3 (双大核)。负责 NCNN INT8 QAT 量化模型的高速矩阵推理、
 *            SDOT 硬件指令加速、卡尔曼滤波轨迹预测及反向 IoU 标签挂载。
 */
#include "inference_node.hpp"
#include "global_context.hpp"
#include "core_config.hpp"
#include "ui_main_window.hpp"
#include "rpmsg_node.hpp"
#include "net.h"
#include "cpu.h"
#include <thread>
#include <chrono>
#include <map>
#include "ByteTrack/BYTETracker.h" 

// 全局记忆库：用于防抖和状态追踪
std::map<int, bool> alarmed_ids; 
std::map<int, int> track_id_to_label; 

void inference_thread_func() {
    // 1.约束当前主线程绑核大核(2&3)
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(CORE_YOLO_0, &cpuset); CPU_SET(CORE_YOLO_1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // 2.初始化NCNN推理引擎
    ncnn::Net ppe_net;
    ncnn::Option opt;
    opt.num_threads = 2; 
    
    // ==========================================
    // 强制NCNN内部线程池只在2和3核上运行彻底避开1号核
    // ==========================================
    ncnn::CpuSet ncnn_cpuset;
    ncnn_cpuset.disable_all(); // 先清空默认映射
    ncnn_cpuset.enable(2);     // 注入2号核
    ncnn_cpuset.enable(3);     // 注入3号核
    ncnn::set_cpu_thread_affinity(ncnn_cpuset); 
    
    opt.lightmode = true; 
    opt.use_winograd_convolution = true; 
    opt.use_sgemm_convolution = true; 
    opt.use_int8_inference = true; 
    opt.use_int8_packed = true;
    opt.use_int8_storage = true;
    ppe_net.opt = opt;
    
    // 3. 加载INT8量化模型
    if (ppe_net.load_param("../model/model1_int8.param") != 0 || 
        ppe_net.load_model("../model/model1_int8.bin") != 0) {
        printf("\n🚨 致命错误：模型加载失败！请检查 ../model 路径下是否存在 param 和 bin 文件\n");
        is_running = false; return;
    }

    // 4. 初始化ByteTrack追踪器
    byte_track::BYTETracker tracker(30, 30); 
    std::vector<int> valid_class_ids = {0, 1, 2, 3, 4, 5, 6, 7};
    float nms_threshold = 0.45f;

    // 【内存级优化】：将容器声明移到循环外，预分配内存，消灭动态扩容带来的系统调优惩罚
    std::vector<cv::Rect> boxes; boxes.reserve(50);
    std::vector<float> confs; confs.reserve(50);
    std::vector<int> class_ids; class_ids.reserve(50);
    std::vector<byte_track::Object> objects; objects.reserve(50);

    // 5. 核心推理循环
    while (is_running) {
        cv::Mat frame;
        // 从无锁队列拉取画面，非阻塞式等待
        if (!cap_queue.pop(frame)) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

        // 清理上一帧残留的数据，实现内存就地复用
        boxes.clear();
        confs.clear();
        class_ids.clear();
        objects.clear();

        // 预防极端高分辨率撑爆内存
        if (frame.cols > 640 || frame.rows > 480) {
            cv::resize(frame, frame, cv::Size(640, 480));
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // NCNN 前处理
        ncnn::Mat in = ncnn::Mat::from_pixels_resize(frame.data, ncnn::Mat::PIXEL_BGR2RGB, frame.cols, frame.rows, INPUT_SIZE, INPUT_SIZE);
        const float norm_vals[3] = {1/255.f, 1/255.f, 1/255.f};
        in.substract_mean_normalize(0, norm_vals);

        // NCNN 推理执行
        ncnn::Extractor ex = ppe_net.create_extractor();
        ex.input("images", in); ncnn::Mat out; ex.extract("output0", out);

        float sx = (float)frame.cols / INPUT_SIZE; float sy = (float)frame.rows / INPUT_SIZE;

        // 后处理：解析输出 Tensor
        if (!out.empty()) {
            for (int i = 0; i < out.w; i++) {
                float max_prob = 0.f; int max_id = -1;
                for (int c : valid_class_ids) {
                    if (4 + c >= out.h) continue; 
                    float prob = out.row(4 + c)[i];
                    if (prob > max_prob) { max_prob = prob; max_id = c; }
                }
                
                if (max_prob > 0.45f) { 
                    float cx = out.row(0)[i], cy = out.row(1)[i], w = out.row(2)[i], h = out.row(3)[i];
                    boxes.push_back(cv::Rect((cx - w/2.f)*sx, (cy - h/2.f)*sy, w*sx, h*sy));
                    confs.push_back(max_prob);
                    class_ids.push_back(max_id);
                }
            }
        }

        // NMS 非极大值抑制
        std::vector<int> picked;
        cv::dnn::NMSBoxes(boxes, confs, 0.3f, nms_threshold, picked);

        // 构建 ByteTrack 追踪对象
        for (int idx : picked) {
            byte_track::Rect<float> rect(boxes[idx].x, boxes[idx].y, boxes[idx].width, boxes[idx].height);
            byte_track::Object obj(rect, class_ids[idx], confs[idx]);
            objects.push_back(obj);
        }

        // 更新卡尔曼滤波轨迹
        std::vector<byte_track::BYTETracker::STrackPtr> output_stracks = tracker.update(objects);
        auto now = std::chrono::system_clock::now();

        // 渲染与违规判定
        for (const auto& track : output_stracks) {
            auto bbox = track->getRect();
            cv::Rect rect(bbox.x(), bbox.y(), bbox.width(), bbox.height());
            int track_id = track->getTrackId();
            float score = track->getScore(); 

            int label = 0;
            float max_iou = 0.0f;
            
            // 使用 IoU 匹配赋予 Tracker 正确的标签
            for (const auto& obj : objects) {
                cv::Rect obj_rect(obj.rect.x(), obj.rect.y(), obj.rect.width(), obj.rect.height());
                cv::Rect intersect = rect & obj_rect;
                float union_area = rect.area() + obj_rect.area() - intersect.area();
                float iou = (union_area < 1e-5) ? 0.0f : ((float)intersect.area() / union_area);

                if (iou > max_iou) {
                    max_iou = iou;
                    label = obj.label; 
                }
            }

            if (max_iou > 0.3f) {
                track_id_to_label[track_id] = label; 
            } else if (track_id_to_label.find(track_id) != track_id_to_label.end()) {
                label = track_id_to_label[track_id]; 
            }

            // UI 渲染画框
            cv::Scalar color = CLASS_COLORS[label % NUM_CLASSES];
            cv::rectangle(frame, rect, color, 2);
            
            std::string class_name = CLASS_NAMES[label];
            char text[64];
            sprintf(text, "ID:%d %s (%.0f%%)", track_id, class_name.c_str(), score * 100); 
            int text_y = std::max(25, rect.y - 5);
            cv::putText(frame, text, cv::Point(rect.x, text_y), cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);

            // 违规行为触发逻辑 (假设 label 4~7 是违规状态)
            if (label >= 4 && label <= 7) { 
                std::string v_name = CLASS_NAMES[label];

                // 状态机防抖：如果该 ID 是首次报警，或者之前没报过这个类型的警
                if (alarmed_ids.find(track_id) == alarmed_ids.end() || !alarmed_ids[track_id]) {
                auto it = alarmed_ids.find(track_id);
                if (it == alarmed_ids.end() || !it->second) {
                    
                    // 无锁队列如果满载主动覆盖（挤压淘汰机制）
                    AlarmEvent event = {frame.clone(), v_name, now, now, 0};
                    if (!alarm_queue.push(event)) {
                        AlarmEvent trash;
                        alarm_queue.pop(trash);
                        alarm_queue.push(event);
                    }
                    alarmed_ids[track_id] = true; 

                    auto now_c_log = std::chrono::system_clock::to_time_t(now);
                    char time_str_display[64], time_str_file[64];
                    std::strftime(time_str_display, sizeof(time_str_display), "%H:%M:%S", std::localtime(&now_c_log));
                    std::strftime(time_str_file, sizeof(time_str_file), "%Y%m%d_%H%M%S", std::localtime(&now_c_log));
                    
                    // 🔴 迁移提醒：如果用户名变了，请记得修改这里的绝对路径！
                    std::string expected_img_path = "/home/user/ppe/ppe_system/violations_data/" + std::string(time_str_file) + "_" + v_name + ".jpg";

                    // 跨线程发送信号，更新 Qt 界面违规日志
                    emit SignalBridge::getInstance()->sendAlarmLog(
                        QString::fromStdString(v_name), 
                        QString(time_str_display),
                        QString::fromStdString(expected_img_path)
                    );

              printf("🔔 [报警] ID:%d 发生 %s 违规，触发抓拍！置信度: %.0f%%\n", track_id, v_name.c_str(), score * 100);
                }
            }
            // ==========================================
            // ⚠️ 警告：这是被你误删的“合规洗白”逻辑，我给你补回来了！
            // 缺少它，蜂鸣器将永远无法停止！
            // ==========================================
            else if (label >= 0 && label <= 3) {
                if (alarmed_ids.find(track_id) != alarmed_ids.end() && alarmed_ids[track_id]) {
                    alarmed_ids[track_id] = false; 
                auto it = alarmed_ids.find(track_id);
                if (it != alarmed_ids.end() && it->second) {
                    it->second = false; 
                    printf("✅ [合规] ID:%d 已恢复合规，报警锁解除，重新进入监控状态。\n", track_id);
                }
            }
        } // <--- 极其关键！这是结束 for (const auto& track : output_stracks) 的括号！

        // ==========================================
        // 【新增】：全局违规统计与蜂鸣器硬件联动
        // 必须放在 for 循环外面，发送给 UI 之前！
        // ==========================================
        int current_violators = 0;
        for (const auto& track : output_stracks) {
            int tid = track->getTrackId();
            if (alarmed_ids.find(tid) != alarmed_ids.end() && alarmed_ids[tid]) {
                current_violators++;
            }
        }

        // 硬件指令下发
        if (current_violators > 0) {
            RPMsgController::getInstance().set_buzzer(true);
        } else {
            RPMsgController::getInstance().set_buzzer(false);
        }
        // ==========================================

        auto end_time = std::chrono::high_resolution_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        printf("⏱️ [AI 引擎] YOLO推理与追踪耗时: %ld ms | 跟踪目标数: %lu\n", cost, output_stracks.size());

        // 【优化落实】：极致零拷贝 (Zero-Copy)，利用智能指针直接将显存交给主线程 UI 渲染
       // ==========================================
        // 【异构融合】：物理探头强制挟持 AI 视频流
        // ==========================================
        // YOLO 根本不知道有火，它是被底层 RPMsg 状态强行触发的！
        if (RPMsgController::getInstance().is_physical_alarm) {
            
            

            // 2. 强行取证：绕过 AI，直接向 IO 线程投递抓拍任务！(加 3 秒防抖防止硬盘爆炸)
            static auto last_fire_snap = std::chrono::system_clock::now() - std::chrono::seconds(10);
            auto now_p = std::chrono::system_clock::now();
            
            if (std::chrono::duration_cast<std::chrono::seconds>(now_p - last_fire_snap).count() > 3) {
                AlarmEvent event = {frame.clone(), "底层火警探头", now_p, now_p, 0};
                if (!alarm_queue.push(event)) {
                    AlarmEvent trash; alarm_queue.pop(trash); alarm_queue.push(event);
                }
                last_fire_snap = now_p;
                printf("📸 [异构联动] 物理火警已触发，强制抓拍留存证据！\n");
                
                // 3. 强制更新 UI 日志表格
                auto now_c_log = std::chrono::system_clock::to_time_t(now_p);
                char time_str_display[64], time_str_file[64];
                std::strftime(time_str_display, sizeof(time_str_display), "%H:%M:%S", std::localtime(&now_c_log));
                std::strftime(time_str_file, sizeof(time_str_file), "%Y%m%d_%H%M%S", std::localtime(&now_c_log));
                
                std::string expected_img_path = "/home/user/ppe/ppe_system/violations_data/" + std::string(time_str_file) + "_底层火警探头.jpg";
                
                emit SignalBridge::getInstance()->sendAlarmLog(
                    QString("底层物理火警"), 
                    QString(time_str_display),
                    QString::fromStdString(expected_img_path)
                );
            }
        }

        // 【优化落实】：极致零拷贝 (Zero-Copy)，利用智能指针直接将显存交给主线程 UI 渲染
        emit SignalBridge::getInstance()->sendFrame(frame);

        // 系统内存守护：定期清理过期的历史追踪 ID
        if (alarmed_ids.size() > 5000) {
            std::map<int, bool> active_alarms;
            std::map<int, int> active_labels;
            for (const auto& track : output_stracks) {
                int tid = track->getTrackId();
                if (alarmed_ids.count(tid)) active_alarms[tid] = alarmed_ids[tid];
                if (track_id_to_label.count(tid)) active_labels[tid] = track_id_to_label[tid];
            }
            alarmed_ids = active_alarms;
            track_id_to_label = active_labels;
            printf("🧹 [系统守护] 记忆库达上限，已精准清理离开画面的历史 ID，释放内存！\n");
        }
    }
}
