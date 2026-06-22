# 飞腾派 E2000Q 异构多核 PPE 智能监控系统 - 源码交付包

## 1. 工程结构拓扑与导览

尊敬的评委专家： 您好。本压缩包包含了基于飞腾派 E2000Q（1+2+1 异构多核架构）开发的 PPE 智能监控系统全栈源码。 为彻底解决传统边缘设备算力拥塞与硬件响应延迟的痛点，本团队对系统底层设备树（DTS）进行了重构，实现了“Linux 智能计算”与“Bare-metal 物理干预”的算力物理隔离。

为保障评审专家查阅代码的逻辑顺畅，源码包被严格划分为两个平行的系统域：

### 📁 `ppe_system/` (Linux 主核系统域)

- **运行宿主**：Core 0 (UI/IO) + Core 2/3 (AI矩阵)
- **核心内容**：包含基于 NCNN 的 QAT 量化推理引擎、SPSC 无锁环形队列管线、Qt5 零拷贝渲染机制，以及处理跨核 RPMsg 异步通信的底层封装。
- **工程注记**：为保障底层 `cv::imwrite` 与 `statvfs` 防洪自愈引擎的绝对物理路径（Hardcoded Absolute Paths）在评委复现环境中的 I/O 稳定性，此顶层目录名 `ppe_system` 保持原始开发基线命名，未做修改。

### 📁 `Baremetal_Slave_Node/` (从核裸机域)

- **运行宿主**：Core 1 (脱离 Linux 调度)
- **核心内容**：纯 C 语言编写的裸机固件源码。包含绕开 OS 调度的 GPIO/EXTI 微秒级中断接管逻辑，以及 OpenAMP 通信前哨站机制。
- **详细文档**：此目录内附有专属的《异构多核硬实时前哨站开发说明》，详述了裸机环境的交叉编译与总线规范。

---

## 2. 异构系统加载与编译整合说明

本系统在运行态下，两个平行域通过共享内存完成物理级会师：

1. **从核编译**：在 `Baremetal_Slave_Node` 环境下使用 `arm-none-eabi-gcc` 编译生成的固件 (`pe2204_aarch64_phytiumpi_openamp_core0.elf`)，已作为预编译资产，默认下发至主核。
2. **主核唤醒**：Linux 侧的脚本通过 OS 底层的 `remoteproc` 框架，将该固件精确投递至划定的物理内存空间，唤醒 Core 1 进入硬实时战备状态。
3. **主核编译**：`ppe_system` 需在 AArch64 环境下依赖 OpenCV 4.x 与 Qt5 环境执行 CMake 构建。

---

## 3. 系统技术文档全景索引 (Technical Documentation Index)

本系统配备了完整的技术文档套件，包含操作、架构、任务流、调试日志及安全分析等，供评审专家与工程复现团队快速检索：

| 文档链接 | 文档定位与核心特色描述 |
| :--- | :--- |
| [📖 docs/operations-guide.md](docs/operations-guide.md) | ★ **操作手册**：包含完整的 AArch64 Linux 环境依赖、Baremetal 编译部署路径、TC-01~14 测试流程，以及心跳挂死冷重启 FIT 故障注入测试的实操指南。 |
| [📖 docs/architecture.md](docs/architecture.md) | ★ **架构全景图**：详细梳理了 E2000Q J2 排针的电气物理布局、主从核物理内存隔离分配表（`0x80000000` 与 `0xB0100000` 共享段）、全栈源码关键文件的逻辑关系与双向数据流拓扑（Mermaid 图）。 |
| [📖 docs/baremetal-task-flow.md](docs/baremetal-task-flow.md) | ★ **从核任务流程**：解析 Core 1 裸机运行下的初始化流程、四态状态机切换逻辑、优先级为 0 的 EXTI 物理硬中断微秒级响应推导，以及硬件看门狗（FWDT0）自愈时序。 |
| [📖 docs/debug-log.md](docs/debug-log.md) | **调试日志**：针对异构多核架构，精心总结了项目开发过程中的典型 Bug，包含多线程无锁队列、跨编译单元类布局错位、内存对齐填充、传感器阻塞挂载等，彰显扎实的工程量。 |
| [📖 docs/communication-flow.md](docs/communication-flow.md) | **通信流程详解**：拆解 OpenAMP 共享内存及 SGI 中断唤醒机制，定义统一的结构体字节对齐数据帧，给出了主从双侧完全对齐的 **CRC-8/MAXIM** 校验算法源码与心跳断连判决时序。 |
| [📖 docs/knowledge-base.md](docs/knowledge-base.md) | **知识库**：沉淀飞腾底层 SDK 技术，包含 FGpio、FI2c 和 FWdt 驱动 API，梳理内核中 `remoteproc` 虚拟字符设备驱动框架以及 Cortex-A72 NEON 指令集向量化加速的底层寄存器重拍机制。 |
| [📖 docs/setup-guide.md](docs/setup-guide.md) | **部署指南**：规范主从核交叉编译工具链设置，整理 `DEEPSEEK_API_KEY`、`DEEPSEEK_MOCK_OFFLINE` 等安全降级与离线故障注入测试的环境变量表，并介绍 `temp_helper` 中一键上传/编译/重启从核的自动化工具链。 |
| [📖 docs/sensor-real-hardware-接入指南.md](docs/sensor-real-hardware-接入指南.md) | **接线指南**：给出详细的物理针脚与传感器（火焰、MQ-2 气体、AHT20/DHT11 温湿度、声光报警器）物理接线映射图。**对比 LoRa 等无线系统，详细陈述了本方案在工业环境下“硬中断穿透、无无线延时、抗电磁干扰”的核心竞争优势**。 |
| [📖 docs/optimization-record.md](docs/optimization-record.md) | **性能优化记录**：用详实的实测数据矩阵，记录了多核大核亲和性绑定、NEON-SDOT 硬件乘加、无锁队列（相较于互斥锁时延由 52us 骤降至 400ns）以及 Qt 双缓冲区指针交换零拷贝渲染的极限加速效果。 |
| [📖 docs/test_coverage_matrix.md](docs/test_coverage_matrix.md) | **需求与测试追溯矩阵**：对齐 14 项功能需求（REQ-01~14）与 14 项功能测试用例（TC-01~14）的 100% 覆盖率追溯矩阵及执行结果。 |
| [📖 docs/risk_analysis.md](docs/risk_analysis.md) | **风险分析与管控**：针对跨核通信中断、主控跑飞等 8 大潜在失效模式，提供完整的 FMEA 缓释手段设计与故障注入（FIT）测试设计。 |
| [📖 docs/safety_design.md](docs/safety_design.md) | **安全可靠性设计说明**：系统多物理域隔离理念、心跳双向监督逻辑、多级优雅降级策略及失效即安全（Fail-Safe）的整体可靠性设计阐述。 |

---

*版权申明：本系统中涉及的并发锁消除、Zero-Copy 内存池、微架构绑核约束及基于 RPMsg 的双向防抖状态机，均为团队成员一行行手工重构。感谢您的评审！*
