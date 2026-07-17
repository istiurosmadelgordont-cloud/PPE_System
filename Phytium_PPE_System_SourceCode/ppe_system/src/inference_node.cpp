/**
 * @file      inference_node.cpp
 * @brief     异构视觉推理大脑与 ByteTrack 追踪引擎
 * @author    [双生序章]
 * @version   2.1.0
 * @date      2026-05-18
 * @note      物理绑核于 Core 2 & 3 (双大核)。负责 NCNN INT8 QAT
 * 量化模型的高速矩阵推理、 SDOT 硬件指令加速、卡尔曼滤波轨迹预测及反向 IoU
 * 标签挂载。
 */
#include "inference_node.hpp"
#include "ByteTrack/BYTETracker.h"
#include "core_config.hpp"
#include "cpu.h"
#include "global_context.hpp"
#include "net.h"
#include "rpmsg_node.hpp"
#include "ui_main_window.hpp"
#include <chrono>
#include <thread>
#include <unordered_map>

// 【优化 1】：全局记忆库改用 unordered_map，O(1) 哈希查找替代 O(log n) 红黑树
std::unordered_map<int, bool> alarmed_ids;
std::unordered_map<int, int> track_id_to_label;
std::unordered_map<int, int> violation_streak;   // 每个追踪ID的连续违规帧数计数
std::unordered_map<int, int> compliance_streak;  // 每个追踪ID的连续合规帧数计数

bool verify_file_md5(const std::string& filepath, const std::string& expected_md5) {
    std::string cmd = "md5sum " + filepath + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != nullptr)
            result += buffer;
    }
    pclose(pipe);
    if (result.length() < 32) return false;
    std::string actual_md5 = result.substr(0, 32);
    std::transform(actual_md5.begin(), actual_md5.end(), actual_md5.begin(), ::tolower);
    std::string target_md5 = expected_md5;
    std::transform(target_md5.begin(), target_md5.end(), target_md5.begin(), ::tolower);
    return actual_md5 == target_md5;
}

void inference_thread_func() {
  // 1.约束当前主线程绑核大核(2&3)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(CORE_YOLO_0, &cpuset);
  CPU_SET(CORE_YOLO_1, &cpuset);
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

  // 3. 加载INT8量化模型前进行 MD5 完整性校验与自愈
  bool bin_ok = verify_file_md5("../model/model1_int8.bin", "fea590f2f1f743979c8247be39c34b0b");
  bool param_ok = verify_file_md5("../model/model1_int8.param", "e65061a8d0b9e4344b2946a06e58f51b");
  if (!bin_ok || !param_ok) {
    printf("\n🚨 [AI 引擎] 检测到模型损坏或不存在，正在启动自动拉取自愈脚本...\n");
    int ret = system("bash ../scripts/pull_model.sh");
    (void)ret;
    // 重新验证
    bin_ok = verify_file_md5("../model/model1_int8.bin", "fea590f2f1f743979c8247be39c34b0b");
    param_ok = verify_file_md5("../model/model1_int8.param", "e65061a8d0b9e4344b2946a06e58f51b");
    if (!bin_ok || !param_ok) {
      printf("\n🚨 [AI 引擎] 致命错误：模型自愈失败，请检查网络连接及远程 GitHub 仓库！\n");
      is_running = false;
      return;
    }
    printf("✅ [AI 引擎] 模型自愈成功！\n");
  }

  if (ppe_net.load_param("../model/model1_int8.param") != 0 ||
      ppe_net.load_model("../model/model1_int8.bin") != 0) {
    printf("\n🚨 致命错误：模型文件损坏，加载失败！\n");
    is_running = false;
    return;
  }

  // 4. 初始化ByteTrack追踪器
  byte_track::BYTETracker tracker(30, 30);
  std::vector<int> valid_class_ids = {0, 1, 2, 3, 4, 5, 6, 7};
  float nms_threshold = 0.45f;

  // 【内存级优化】：将容器声明移到循环外，预分配内存，消灭动态扩容带来的系统调优惩罚
  std::vector<cv::Rect> boxes;
  boxes.reserve(50);
  std::vector<float> confs;
  confs.reserve(50);
  std::vector<int> class_ids;
  class_ids.reserve(50);
  std::vector<byte_track::Object> objects;
  objects.reserve(50);

  // 5. 核心推理循环
  while (is_running) {
    cv::Mat frame;
    // 从无锁队列拉取画面，非阻塞式等待
    if (!cap_queue.pop(frame)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

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
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        frame.data, ncnn::Mat::PIXEL_BGR2RGB, frame.cols, frame.rows,
        INPUT_SIZE, INPUT_SIZE);
    const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
    in.substract_mean_normalize(0, norm_vals);

    // NCNN 推理执行
    ncnn::Extractor ex = ppe_net.create_extractor();
    ex.input("images", in);
    ncnn::Mat out;
    ex.extract("output0", out);

    float sx = (float)frame.cols / INPUT_SIZE;
    float sy = (float)frame.rows / INPUT_SIZE;

    // 后处理：解析输出 Tensor
    if (!out.empty()) {
      for (int i = 0; i < out.w; i++) {
        float max_prob = 0.f;
        int max_id = -1;
        for (int c : valid_class_ids) {
          if (4 + c >= out.h)
            continue;
          float prob = out.row(4 + c)[i];
          if (prob > max_prob) {
            max_prob = prob;
            max_id = c;
          }
        }

        if (max_prob > 0.45f) {
          float cx = out.row(0)[i], cy = out.row(1)[i], w = out.row(2)[i],
                h = out.row(3)[i];
          boxes.push_back(cv::Rect((cx - w / 2.f) * sx, (cy - h / 2.f) * sy,
                                   w * sx, h * sy));
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
      byte_track::Rect<float> rect(boxes[idx].x, boxes[idx].y, boxes[idx].width,
                                   boxes[idx].height);
      byte_track::Object obj(rect, class_ids[idx], confs[idx]);
      objects.push_back(obj);
    }

    // 更新卡尔曼滤波轨迹
    std::vector<byte_track::BYTETracker::STrackPtr> output_stracks =
        tracker.update(objects);
    auto now = std::chrono::system_clock::now();

    // 【优化 2】：预缓存 NMS 后的 cv::Rect，避免在 IoU 匹配内层循环中反复构造
    std::vector<cv::Rect> obj_rects;
    obj_rects.reserve(objects.size());
    for (const auto &obj : objects) {
      obj_rects.emplace_back(obj.rect.x(), obj.rect.y(), obj.rect.width(),
                             obj.rect.height());
    }

    // ==========================================
    // 渲染与违规判定
    // 【Bug 修复】：彻底重构括号嵌套，消灭所有重复的 find() 残留
    // ==========================================
    for (const auto &track : output_stracks) {
      auto bbox = track->getRect();
      cv::Rect rect(bbox.x(), bbox.y(), bbox.width(), bbox.height());
      int track_id = track->getTrackId();
      float score = track->getScore();

      int label = 0;
      float max_iou = 0.0f;

      // 使用预缓存的 obj_rects 进行 IoU 匹配
      for (size_t oi = 0; oi < objects.size(); oi++) {
        cv::Rect intersect = rect & obj_rects[oi];
        float union_area =
            rect.area() + obj_rects[oi].area() - intersect.area();
        float iou =
            (union_area < 1e-5) ? 0.0f : ((float)intersect.area() / union_area);

        if (iou > max_iou) {
          max_iou = iou;
          label = objects[oi].label;
        }
      }

      // 标签持久化：IoU 匹配成功则更新，否则沿用历史标签
      if (max_iou > 0.3f) {
        track_id_to_label[track_id] = label;
      } else {
        auto it = track_id_to_label.find(track_id);
        if (it != track_id_to_label.end()) {
          label = it->second;
        }
      }

      // UI 渲染画框
      cv::Scalar color = CLASS_COLORS[label % NUM_CLASSES];
      cv::rectangle(frame, rect, color, 2);

      std::string class_name = CLASS_NAMES[label];
      char text[64];
      sprintf(text, "ID:%d %s (%.0f%%)", track_id, class_name.c_str(),
              score * 100);
      int text_y = std::max(25, rect.y - 5);
      cv::putText(frame, text, cv::Point(rect.x, text_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);

      // ==========================================
      // 违规行为触发逻辑 (label 4~7 是违规状态)
      // ==========================================
      if (label >= 4 && label <= 7) {
        std::string v_name = CLASS_NAMES[label];

        compliance_streak[track_id] = 0; // 违规时，清空合规连续计数
        violation_streak[track_id]++;    // 违规连续计数累加

        auto it = alarmed_ids.find(track_id);
        bool is_already_alarmed = (it != alarmed_ids.end() && it->second);

        // 未锁定警报 且 连续违规帧数 >= 3 帧 → 触发锁定！
        if (!is_already_alarmed && violation_streak[track_id] >= 3) {
          // 遵守 SPSC 规则：无锁队列满载时丢弃新报警事件，避免生产者调用 pop() 导致头指针损坏（Segmentation fault）
          AlarmEvent event = {frame.clone(), v_name, now, now, 0};
          if (!alarm_queue.push(event)) {
            printf("⚠️ [报警] IO队列已满，丢弃该次防抖确认违规事件。\n");
          }
          alarmed_ids[track_id] = true;
          violation_streak[track_id] = 0; // 触发后清空计数

          auto now_c_log = std::chrono::system_clock::to_time_t(now);
          char time_str_display[64], time_str_file[64];
          std::strftime(time_str_display, sizeof(time_str_display), "%H:%M:%S",
                        std::localtime(&now_c_log));
          std::strftime(time_str_file, sizeof(time_str_file), "%Y%m%d_%H%M%S",
                        std::localtime(&now_c_log));

          // 🔴 迁移提醒：如果用户名变了，请记得修改这里的绝对路径！
          std::string expected_img_path =
              "/home/user/ppe/ppe_system/violations_data/" +
              std::string(time_str_file) + "_" + v_name + ".jpg";

          // 跨线程发送信号，更新 Qt 界面违规日志
          emit SignalBridge::getInstance() -> sendAlarmLog(
              QString::fromStdString(v_name), QString(time_str_display),
              QString::fromStdString(expected_img_path));

          printf("🔔 [防抖警报锁死] ID:%d 连续 3 帧 %s 违规，触发抓拍！置信度: %.0f%%\n",
                 track_id, v_name.c_str(), score * 100);
        }
      }
      // ==========================================
      // 合规洗白逻辑：连续 15 帧合规才洗白恢复
      // ==========================================
      else if (label >= 0 && label <= 3) {
        violation_streak[track_id] = 0;  // 合规时，清空违规连续计数
        compliance_streak[track_id]++;   // 合规连续计数累加

        auto it = alarmed_ids.find(track_id);
        bool is_already_alarmed = (it != alarmed_ids.end() && it->second);

        // 已锁定警报 且 连续合规帧数 >= 15 帧 → 解锁洗白！
        if (is_already_alarmed && compliance_streak[track_id] >= 15) {
          it->second = false;
          compliance_streak[track_id] = 0; // 触发后清空计数
          printf("✅ [防抖合规洗白] ID:%d 连续 15 帧合规，报警锁定状态洗白解除。\n",
                 track_id);
        }
      }
    } // <--- for (const auto& track : output_stracks) 结束

    // ==========================================
    // 全局违规统计与蜂鸣器硬件联动
    // 【Bug 修复】：移到 for 循环之后，并直接基于 label 判定，
    // 避免 alarmed_ids 在首帧尚未设置时 current_violators 为 0 的时序Bug
    // ==========================================
    int current_violators = 0;
    for (const auto &track : output_stracks) {
      int tid = track->getTrackId();
      // 方式1：alarmed_ids 中存在且为 true
      auto it = alarmed_ids.find(tid);
      if (it != alarmed_ids.end() && it->second) {
        current_violators++;
      }
    }

    // 硬件指令下发与 UI 状态联动
    bool has_ai_violation = (current_violators > 0);
    if (has_ai_violation) {
      RPMsgController::getInstance().set_buzzer(true);
      printf("[AI报警信号] current_violators=%d, 发送 sendAiAlarmStatus(TRUE)\n", current_violators);
      fflush(stdout);
    } else {
      RPMsgController::getInstance().set_buzzer(false);
    }
    emit SignalBridge::getInstance() -> sendAiAlarmStatus(has_ai_violation);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time)
                    .count();
    printf("⏱️ [AI 引擎] YOLO推理与追踪耗时: %ld ms | 跟踪目标数: %lu\n", cost,
           output_stracks.size());

    // ==========================================
    // 【异构融合】：物理探头强制挟持 AI 视频流
    // ==========================================
    if (RPMsgController::getInstance().is_physical_alarm) {
      // 强行取证：绕过 AI，直接向 IO 线程投递抓拍任务！(加 3
      // 秒防抖防止硬盘爆炸)
      static auto last_fire_snap =
          std::chrono::system_clock::now() - std::chrono::seconds(10);
      auto now_p = std::chrono::system_clock::now();

      if (std::chrono::duration_cast<std::chrono::seconds>(now_p -
                                                           last_fire_snap)
              .count() > 3) {
        AlarmEvent event = {frame.clone(), "底层火警探头", now_p, now_p, 0};
        if (!alarm_queue.push(event)) {
          printf("⚠️ [报警] IO队列已满，丢弃火警事件抓拍。\n");
        }
        last_fire_snap = now_p;
        printf("📸 [异构联动] 物理火警已触发，强制抓拍留存证据！\n");

        // 强制更新 UI 日志表格
        auto now_c_log = std::chrono::system_clock::to_time_t(now_p);
        char time_str_display[64], time_str_file[64];
        std::strftime(time_str_display, sizeof(time_str_display), "%H:%M:%S",
                      std::localtime(&now_c_log));
        std::strftime(time_str_file, sizeof(time_str_file), "%Y%m%d_%H%M%S",
                      std::localtime(&now_c_log));

        std::string expected_img_path =
            "/home/user/ppe/ppe_system/violations_data/" +
            std::string(time_str_file) + "_底层火警探头.jpg";

        emit SignalBridge::getInstance()
            -> sendAlarmLog(QString("底层物理火警"), QString(time_str_display),
                            QString::fromStdString(expected_img_path));
      }
    }

    // 零拷贝渲染：直接将帧交给主线程 UI
    // 违规数量只上报已通过连续帧确认的违规目标，不再把普通检测目标计入。
    emit SignalBridge::getInstance() -> sendAiMetrics((int)cost, current_violators);
    emit SignalBridge::getInstance() -> sendFrame(frame);

    // 系统内存守护：定期清理过期的历史追踪 ID
    if (alarmed_ids.size() > 5000) {
      std::unordered_map<int, bool> active_alarms;
      std::unordered_map<int, int> active_labels;
      std::unordered_map<int, int> active_violation_streak;
      std::unordered_map<int, int> active_compliance_streak;
      for (const auto &track : output_stracks) {
        int tid = track->getTrackId();
        if (alarmed_ids.count(tid))
          active_alarms[tid] = alarmed_ids[tid];
        if (track_id_to_label.count(tid))
          active_labels[tid] = track_id_to_label[tid];
        if (violation_streak.count(tid))
          active_violation_streak[tid] = violation_streak[tid];
        if (compliance_streak.count(tid))
          active_compliance_streak[tid] = compliance_streak[tid];
      }
      alarmed_ids = std::move(active_alarms);
      track_id_to_label = std::move(active_labels);
      violation_streak = std::move(active_violation_streak);
      compliance_streak = std::move(active_compliance_streak);
      printf("🧹 [系统守护] 记忆库达上限，已精准清理离开画面的历史 "
             "ID 并重置防抖状态，释放内存！\n");
    }
  }
}
