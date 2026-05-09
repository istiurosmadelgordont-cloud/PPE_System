#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <chrono>
#include <string>

// ==========================================
// 硬件级物理核映射 (飞腾派)
// ==========================================
#define CORE_CAM    0  // 0号小核：负责拉流/视频解码/UI界面
#define CORE_IO     0  // 0号小核：负责硬盘写盘 (挤在0号，为了给从核腾位置)
#define CORE_SLAVE  1  // 1号小核：【严格预留】作为独立传感器从核 (跑RTOS/裸机)
#define CORE_YOLO_0 2  // 2号大核：负责 AI 推理
#define CORE_YOLO_1 3  // 3号大核：负责 AI 推理 (与 2 号共担)

// 算法核心参数
#define INPUT_SIZE 320
#define CONF_THRESH 0.4f
#define NMS_THRESH 0.45f
#define TRACK_INTERVAL 3
#define DEBOUNCE_SEC 0.5f // 连续违规 0.5s 触发防抖报警

// 模型类别精简为 8 类
#define NUM_CLASSES 8

// 追踪与渲染对象
struct TrackedObject {
    cv::Rect_<float> rect;
    int label;
    float prob;
    std::vector<cv::Point2f> points; 
};

// 报警事件载荷
struct AlarmEvent {
    cv::Mat frame;
    std::string violation_type;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point alarm_time;
    float duration;
};

// 1-4 正常(冷色)，5-8 违规(暖色/红色)
const cv::Scalar CLASS_COLORS[NUM_CLASSES] = {
    cv::Scalar(255, 0, 0), cv::Scalar(0, 255, 255), cv::Scalar(0, 165, 255), cv::Scalar(0, 255, 0),
    cv::Scalar(0, 0, 255), cv::Scalar(0, 0, 200), cv::Scalar(130, 0, 75), cv::Scalar(255, 191, 0)
};

const char* const CLASS_NAMES[NUM_CLASSES] = {
    "Helmet", "Glasses", "Safety Vest", "Gloves", 
    "Without Helmet", "Without Glass", "Without Glove", "Without Safety Vest" 
};