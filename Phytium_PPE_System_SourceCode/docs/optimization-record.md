# 📖 性能优化记录 — 算力榨取、并行加速与低拷贝

> **硬件载体**：飞腾派 E2000Q 4核 SoC  
> **加速对象**：NCNN YOLO AI 目标检测、Qt 渲染大屏、跨核 RPMsg 异步通信、无锁并发总线

---

## 1. 核心架构优化实现矩阵

为榨取飞腾派的芯片算力，我们对 AI 视觉推理域与图形渲染域进行了深度的底层重构。本系统包含以下优化项：

| 优化项 | 项目实现 | 工程收益 |
|---|---|---|
| 算力隔离 | 推理及 NCNN 线程池绑定 Core 2/3 | 减少与视频、UI、I/O 的资源竞争 |
| INT8 推理 | NCNN 启用 INT8 inference / packed / storage | 降低边缘 CPU 推理负载 |
| 无锁流转 | SPSC 队列、缓存行隔离、Acquire/Release | 避免锁竞争与无界积压 |
| 低拷贝显示 | `cv::Mat` 内存包装为 `QImage` | 减少中间图像复制 |
| 异步证据存储 | 独立 I/O 线程临时文件写入后原子重命名 | 降低写盘对主业务的影响 |

---

## 2. 推理线程与 CPU 绑核 (Affinity)

飞腾派 E2000Q 采用 FTC664 双大核 + 双小核的非对称架构。
*   **痛点**：若不进行线程约束，Linux 会将 AI 推理随意分配给核心，造成资源竞争。
*   **优化策略**：
    将推理线程和 NCNN 两线程池定向绑定到 Core 2/3，避免与 Core 0 的视频采集、UI 和 I/O 业务竞争 CPU 资源。

---

## 3. ARM NEON 硬件指令集加速

本系统使用的 YOLO AI 模型权重经过 QAT INT8 离线量化重写，能够原生利用 ARMv8-A 的 NEON 协处理器执行加速：

*   **Packed Layout 内存打包**：
    *   在 [inference_node.cpp](file:///d:/飞腾派/CICC1004607+初赛+技术数据(代码类)/ppe4-28/Phytium_PPE_System_SourceCode/ppe_system/src/inference_node.cpp) 中开启 `opt.use_int8_packed = true`。
    *   将常规的通道交错（Channal Interleaved）图像矩阵打包为连续对齐，加速矩阵卷积运算。
*   **INT8 向量化推理**：
    *   NCNN 已启用 INT8 推理与 packed 存储。若需将 SDOT 作为性能结论，还应补充 `-march=armv8-a+dotprod` 编译记录及反汇编中 `sdot` 指令证据。

---

## 4. Qt 低拷贝显示与 SPSC 无锁环形队列

### 4.1 Qt 监控大屏“低拷贝” (Low-Copy)
基于 `cv::Mat` 与 `QImage` 的内存包装减少中间深拷贝；结合异步 I/O 降低 UI 线程的图像处理和写盘压力。

### 4.2 C++11 原子屏障 SPSC 无锁环形队列
多线程之间（`camera_node` -> `inference_node` -> `ui_main_window`）的图像数据分发如果采用互斥锁 (`std::mutex`)，频繁的锁申请会导致性能抖动：
*   **无锁优化**：设计了 [lockfree_queue.hpp](file:///d:/飞腾派/CICC1004607+初赛+技术数据(代码类)/ppe4-28/Phytium_PPE_System_SourceCode/ppe_system/include/lockfree_queue.hpp)，SPSC 无锁环形队列使用 `std::atomic`、Acquire/Release 内存序和 64 字节缓存行对齐，避免锁竞争与无界积压，彻底解决了多线程在高负荷下的假死与性能抖动问题。
