# 飞腾派 E2000Q 异构多核 PPE 智能监控系统 - 源码交付包

## 1. 工程项目简介与设计路线

尊敬的评委专家： 您好。本工程包是基于飞腾派 E2000Q（1+2+1 异构多核处理器）开发的 **异构多核 PPE 智能监控系统**。

为彻底解决传统单系统边缘设备在处理高频 AI 推理时算力拥塞、以及面临内核死锁或 OOM 崩溃时外设失控的痛点，本系统采用 **Linux + Baremetal (AMP) 异构多核架构**。通过重构底层设备树，实现了“Linux 智能计算”与“Bare-metal 物理干预”的算力物理隔离。

*   **Linux 主核系统域** (Core 0, 2, 3)：运行 GUI 看板、异步写盘、摄像头拉流以及基于 NCNN 的 YOLOv8 QAT 量化推理引擎。
*   **Bare-metal 从核裸机域** (Core 1)：完全剥离 Linux 调度，实现火焰传感器极速 EXTI 中断响应（μs 级），执行看门狗（WDT）自愈，以及物理报警器的 Fail-Safe 安全闭环动作。
*   **跨核通信通道**：通过片上共享内存（Shared Memory）与软中断（SGI），由 OpenAMP / RPMsg 协议栈搭建高可靠性双向数据总线。

---

## 2. 项目目录结构

```
Phytium_PPE_System_SourceCode/
├── README.md                           # 项目说明 (本文档)
├── project_memory.md                   # 物理内存配置与设备树更改说明
├── 功能需求表.xlsx                     # REQ-01~14 功能指标追溯表
├── 功能测试表.xlsx                     # TC-01~14 测试方案与实际测试表
├── 03_系统测试覆盖分析报告_终稿.xlsx   # 交付文档 - 测试覆盖评估
├── 04_风险管理闭环设计报告_终稿.xlsx   # 交付文档 - FMEA管控大表
├── 05_高阶风险管理6-17.xlsx           # 交付文档 - 答辩用故障树与风险控制
│
├── ppe_system/                         # 📁 Linux 主核系统域 (C++/Qt)
│   ├── CMakeLists.txt                  # CMake 编译配置文件
│   ├── run_real_deepseek.sh            # 一键拉载从核并运行主进程脚本
│   ├── setup_env.sh                    # 设置大模型API和降级环境变量
│   ├── src/                            # 主核业务源代码
│   │   ├── main.cpp                    # 主程序入口，信号捕获与优雅退出
│   │   ├── camera_node.cpp             # 摄像头图像拉流与无感热切换
│   │   ├── inference_node.cpp          # YOLOv8-NCNN + ByteTrack推理模块
│   │   ├── rpmsg_node.cpp              # RPMsg 主核侧驱动，带心跳与看门人检测
│   │   ├── io_node.cpp                 # 磁盘写入引擎与超85%防洪自清洁
│   │   ├── ui_main_window.cpp          # Qt 零拷贝显示监控大屏与状态灯看板
│   │   └── deepseek_worker.cpp         # DeepSeek AI 顾问网络通信后台
│   └── include/                        # 头文件目录
│       ├── lockfree_queue.hpp          # C++11 单生产单消费无锁队列
│       └── ...
│
├── Baremetal_Slave_Node/               # 📁 从核裸机域 (C)
│   ├── Kconfig                         # 裸机配置参数 Kconfig
│   ├── makefile                        # gcc-arm-none-eabi 编译 Makefile
│   ├── main.c                          # 从核启动入口与硬件总线扫描
│   ├── ft_openamp.ld                   # 链接脚本（绑定于 0x80000000 空间）
│   ├── src/                            # 从核硬件采集源码
│   │   ├── slaver_00_example.c         # OpenAMP 交互循环、看门狗喂狗与自愈
│   │   ├── fire_sensor.c               # 火焰 GPIO 边缘触发 EXTI 中断驱动
│   │   ├── gas_sensor.c                # MQ-2 气体 GPIO 输入驱动
│   │   ├── buzzer.c                    # 物理报警器逻辑驱动与反馈诊断
│   │   ├── aht20.c                     # I2C 温湿度轮询读取驱动
│   │   └── led20set.c                  # GPIO 指示灯驱动
│   └── inc/
│       └── ...
│
├── docs/                               # 📁 9大核心交付技术文档套件
│   ├── operations-guide.md             # 操作手册 (快速编译部署与测试)
│   ├── architecture.md                 # 架构全景图 (排针引脚与共享内存区)
│   ├── baremetal-task-flow.md          # 从核裸机高实时前哨站流程
│   ├── debug-log.md                    # 23个真实项目开发调试记录
│   ├── communication-flow.md           # 跨核 RPMsg 数据包帧与 CRC8 校验源码
│   ├── setup-guide.md                  # 环境变量与交叉工具链指南
│   ├── sensor-real-hardware-接入指南.md # 硬件物理引脚接线映射
│   ├── risk_analysis.md                # 8大失效模式风险分析（FMEA）
│   └── safety_design.md                # 失效安全（Fail-Safe）与优雅降级
│
├── tests/                              # 📁 故障注入与测试执行脚本
│   └── ...                             # fit_*.sh 测试脚本
│
└── temp_helper/                        # 📁 自动化部署与同步管道脚本
    └── ...
```

---

## 3. 硬件平台参数

| 项目 | 详情 |
| :--- | :--- |
| **开发板** | 飞腾派 CEK8903 (Phytium Pi) |
| **SoC** | PE2204 (2 × FTC664 + 2 × FTC310) |
| **处理器架构** | ARM64 (AArch64) |
| **运行操作系统** | Debian 12 (PIOS v3.2) |
| **系统内核版本** | 6.6.63-phytium-embedded-v3.2 |
| **开发板 IP 地址** | 172.20.10.2 / DHCP 自动分配 |
| **默认登录账户** | user / root (密码均为: user) |

---

## 4. CPU 核心分配与调度

飞腾派 E2000Q 处理器具备 4 个核心。为保证硬实时任务不受主核 Linux 系统调度开销影响，系统对其进行了精细化分配：

| CPU 编号 | 核心微架构 | MPIDR | 系统与业务分配用途 |
| :--- | :--- | :--- | :--- |
| **CPU0** | FTC310 (LITTLE) | 0x200 | Linux 主核系统域（执行 GUI 渲染、I/O 存储与摄像头拉流） |
| **CPU1** | **FTC310 (LITTLE)** | **0x201** | **Baremetal 裸机从核域**（运行实时采集中断与看门狗） |
| **CPU2** | FTC664 (big) | 0x000 | Linux 主核系统域（亲和性绑定：专职 NCNN YOLOv8-INT8 推理） |
| **CPU3** | FTC664 (big) | 0x100 | Linux 主核系统域（亲和性绑定：专职 NCNN YOLOv8-INT8 推理） |

> **提示**：从核 Baremetal 实际绑定运行于 CPU1，但底层设备树中 `remote-processor` 管理节点仍标记为 `remoteproc0`，主从核的软硬件加载均由 remoteproc 框架完成。

---

## 5. 跨核通信架构与协议

本系统的跨核通信使用共享内存作为数据链路，配合 SGI 中断，具备极低的时延和极高的可靠性：

```
Linux 侧主核 (CPU 0/2/3)                      从核裸机侧 (CPU 1)
┌──────────────────────┐               ┌──────────────────────────┐
│  Qt UI / AI 推理     │               │  slaver_00主任务 (Prio=4)│
│  /dev/rpmsg0         │   RPMsg 通道  │  ├─ DEVICE_CORE_CHECK ←  │
│  rpmsg_char 驱动     │  ───────────► │  ├─ DEVICE_CORE_BUZZER → │
│  └─ 信号与状态展示   │  ◄───────────  │  └─ 外设自检/故障自愈    │
└──────────┬───────────┘    SGI 9 中断  └──────────────┬───────────┘
           │                                           │
           │      ┌─────────────────────────────┐      │
           └─────►│  共享内存 0xB0100000 (1MB)  │◄─────┘
                  │  vring0 + vring1 + RPMsg缓冲 │
                  └─────────────────────────────┘
```

*   **异构通信链路**：使用片上 1MB 共享物理内存（`0xB0100000` 开始的段）作为数据缓冲区，通信双方在写入数据后通过软中断 `SGI 9` 相互唤醒，避免轮询等待。
*   **通信通道**：建立一个独占的 RPMsg 通道（名为 `"rpmsg-openamp-demo-channel"`），通过数据包帧头部的 `command` 字段实现上行火焰/气体传感器状态上报与下行声光报警器控制指令的双向复用。
*   **完整性保护**：数据帧包尾强制填充 **CRC-8/MAXIM** 校验值（多项式 `0x31`，初值 `0xFF`），凡是在共享内存段中由于现场强电磁干扰导致位翻转（Bit Flip）的数据，接收侧重新计算 CRC8 不匹配时直接丢弃，保障硬件控制指令绝对安全。

---

## 6. 开发资源说明

| 关键开发资源 | 本项目本地路径 / 推荐工具链配置 |
| :--- | :--- |
| **从核 Baremetal SDK** | `phytium-standalone-sdk-master/` (已作为子模块直接置于根目录) |
| **从核裸机编译器** | `arm-none-eabi-gcc` (建议官方 `10.3-2021.10` 版本，需配置进 `PATH`) |
| **主核 Linux 编译器** | 飞腾派本地原生 `g++` / `gcc` (AArch64 Native Compiler) |
| **内核版本支持** | 飞腾派标准嵌入式 Linux 内核（带 `remoteproc` / `rpmsg_char` 支持） |
| **开发板物理引脚接线**| 参见 [📖 docs/sensor-real-hardware-接入指南.md](docs/sensor-real-hardware-接入指南.md) |

---

## 7. 高阶高防灾性故障注入测试 (FIT)

本工程包随附了 5 个高阶故障注入测试脚本（位于 `tests/` 目录），覆盖了系统在面临通信断连、主控死机、强电磁磁场干扰、算法文件损坏和网络波动等工业极端工况下的防御、降级与看门狗物理自愈重建能力：

| 序号 | 测试脚本 / 指标 | 故障注入场景与原理 | 预期系统自愈与安全保护状态 |
| :---: | :--- | :--- | :--- |
| **1** | `fit_rpmsg_disconnect.sh`<br>(TC-09) | **从核通信断连测试**：使用 `echo stop` 强制关停从核（Core 1）。 | **安全防挂与指示置红**：主核通过 remoteproc 状态审计自动挂起 ioctl 动作，避免内核死锁，UI 灯在 1s 内刷新为红色“断开”状态。从核因停止喂狗，10秒后系统被看门狗物理重启自愈。 |
| **2** | `fit_main_crash.sh`<br>(TC-10) | **主进程崩溃死机测试**：在警报鸣响期间，使用 `kill -9` 强杀主进程 `ppe_system`。 | **失效安全 (Fail-Safe)**：从核心跳包计数累计丢失 10s 后，物理断开蜂鸣器供电停止噪音；随后主动停止喂狗，看门狗 1s 后物理复位整机，重新自愈拉起 Linux。 |
| **3** | `fit_crc_corrupt.sh`<br>(TC-11) | **信道数据损坏干扰测试**：使用 `dd` 向物理端口 `/dev/rpmsg0` 灌入大量脏字节。 | **数据安全隔离**：主从双侧均通过 CRC8/MAXIM（多项式 `0x31`）强校验阻断并丢弃脏包。系统完全不崩溃、不发生动作误动，UI 的 CRC 错误计数递增。 |
| **4** | `fit_model_missing.sh`<br>(TC-12) | **AI 核心权重损坏自愈测试**：移除正在运行的 `model1_int8.param` 模型描述文件。 | **文件容灾自愈**：主控启动时 MD5 校验不匹配，自动拉起自愈脚本。若网络异常，脚本自动转入本地备份物理覆盖自愈，保障算法可用性（已加固 `-s` 大小审计，杜绝 0 字节文件污染）。 |
| **5** | `fit_deepseek_offline.sh`<br>(TC-13) | **AI 顾问断网熔断测试**：禁用开发板物理网卡以模拟断网。 | **异步非阻塞降级**：YOLO 推理和 GUI 画面在断网请求期间 100% 流畅不卡顿（后台异步执行）。网络 10s 超时后触发熔断，界面自动无缝降级加载本地高安全方案。 |

---

## 8. 系统技术文档全景索引

本系统配备了完整的技术文档套件，包含操作、架构、任务流、调试日志及安全分析等，供评审专家与工程复现团队快速检索：

| 文档链接 | 文档定位与核心特色描述 |
| :--- | :--- |
| [📖 docs/operations-guide.md](docs/operations-guide.md) | ★ **操作手册**：包含完整的 AArch64 Linux 环境依赖、Baremetal 编译部署路径、TC-01~14 测试流程，以及心跳挂死冷重启 FIT 故障注入测试的实操指南。 |
| [📖 docs/architecture.md](docs/architecture.md) | ★ **架构全景图**：详细梳理了 E2000Q J2 排针的电气物理布局、主从核物理内存隔离分配表（`0x80000000` 与 `0xB0100000` 共享段）、全栈源码关键文件的逻辑关系与双向数据流拓扑（Mermaid 图）。 |
| [📖 docs/baremetal-task-flow.md](docs/baremetal-task-flow.md) | ★ **从核任务流程**：解析 Core 1 裸机运行下的初始化流程、四态状态机切换逻辑、优先级为 0 的 EXTI 物理硬中断微秒级响应推导，以及硬件看门狗（FWDT0）自愈时序。 |
| [📖 docs/debug-log.md](docs/debug-log.md) | **调试日志**：针对异构多核架构，精心总结了项目开发过程中的典型 Bug，包含多线程无锁队列、跨编译单元类布局错位、内存对齐填充、传感器阻塞挂载等，展现了极强的工程调试量。 |
| [📖 docs/communication-flow.md](docs/communication-flow.md) | **通信流程详解**：拆解 OpenAMP 共享内存及 SGI 中断唤醒机制，定义统一的结构体字节对齐数据帧，给出了主从双侧完全对齐的 **CRC-8/MAXIM** 校验算法源码与心跳断连判决时序。 |
| [📖 docs/knowledge-base.md](docs/knowledge-base.md) | **知识库**：沉淀飞腾底层 SDK 技术，包含 FGpio、FI2c 和 FWdt 驱动 API，梳理内核中 `remoteproc` 虚拟字符设备驱动框架以及 Cortex-A72 NEON 指令集向量化加速的底层寄存器重拍机制。 |
| [📖 docs/setup-guide.md](docs/setup-guide.md) | **部署指南**：规范主从核交叉编译工具链设置，整理 `DEEPSEEK_API_KEY`、`DEEPSEEK_MOCK_OFFLINE` 等安全降级与离线故障注入测试的环境变量表，并介绍 `temp_helper` 中一键上传/编译/重启从核的自动化工具链。 |
| [📖 docs/sensor-real-hardware-接入指南.md](docs/sensor-real-hardware-接入指南.md) | **接线指南**：给出详细的物理针脚与传感器（火焰、MQ-2 气体、AHT20/DHT11 温湿度、声光报警器）物理接线映射图。**对比 LoRa 等无线系统，详细陈述了本方案在工业环境下“硬中断穿透、无无线延时、抗电磁干扰”的核心竞争优势**。 |
| [📖 docs/optimization-record.md](docs/optimization-record.md) | **性能优化记录**：用详实的实测数据矩阵，记录了多核大核亲和性绑定、NEON-SDOT 硬件乘加、无锁队列（相较于时延由 52us 骤降至 400ns）以及 Qt 双缓冲区指针交换零拷贝渲染的极限加速效果。 |
| [📖 docs/test_coverage_matrix.md](docs/test_coverage_matrix.md) | **需求与测试追溯矩阵**：对齐 14 项功能需求（REQ-01~14）与 14 项功能测试用例（TC-01~14）的 100% 覆盖率追溯矩阵及执行结果。 |
| [📖 docs/risk_analysis.md](docs/risk_analysis.md) | **风险分析与管控**：针对跨核通信中断、主控跑飞等 8 大潜在失效模式，提供完整的 FMEA 缓释手段设计与故障注入（FIT）测试设计。 |
| [📖 docs/safety_design.md](docs/safety_design.md) | **安全可靠性设计说明**：系统多物理域隔离理念、心跳双向监督逻辑、多级优雅降级策略及失效即安全（Fail-Safe）的整体可靠性设计阐述。 |

---

## 9. 参考链接与规范

*   **飞腾嵌入式 Gitee 文档仓**：[https://gitee.com/phytium_embedded/phytium-embedded-docs](https://gitee.com/phytium_embedded/phytium-embedded-docs)
*   **飞腾 Standalone SDK 源码仓**：[https://gitee.com/phytium_embedded/phytium-standalone-sdk](https://gitee.com/phytium_embedded/phytium-standalone-sdk)
*   **飞腾 FreeRTOS SDK 源码仓**：[https://gitee.com/phytium_embedded/phytium-free-rtos-sdk](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk)
*   **飞腾 OpenAMP 开发指南**：[https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/open-amp](https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/open-amp)
*   **OpenAMP 官方项目组织**：[https://www.openampproject.org/](https://www.openampproject.org/)

---

*版权申明：本系统中涉及的并发锁消除、Zero-Copy 内存池、微架构绑核约束及基于 RPMsg 的双向防抖状态机，均为团队成员一行行手工重构。感谢您的评审！*

**交付版本**: v4.5 | **最新更新**: 2026-06-23 | **系统状态**: 完整移植，AI YOLOv8-NCNN 量化推理 + 从核 Baremetal 前哨实时采集联动 + 双向心跳 WDT0 自愈与物理回路环路探伤已测试完毕
