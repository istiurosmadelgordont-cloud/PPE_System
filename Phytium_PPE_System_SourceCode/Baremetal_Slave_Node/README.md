- # 飞腾派 E2000Q 异构多核硬实时前哨站 (Bare-metal Slave Node)

  ## 1. 架构定位与核心功能

  本工程为工业级 PPE 监控系统的**从核（Bare-metal）物理感知与干预中心**。 它脱离了 Linux 系统的分时调度，运行于飞腾派 E2000Q SoC 的 FTC310（Core 1）物理小核上，旨在提供绝对的微秒级硬实时（Hard Real-Time）响应能力。

  ### 核心特性

  1. **彻底的物理隔离**：基于重构后的设备树（DTS），本系统运行在完全独立分配的物理内存空间（0xb0100000），与主核 Linux OS 在内存寻址、中断路由上彻底解耦。
  2. **极速微架构状态机**：摒弃传统中断下半部推迟机制，在 EXTI（外部中断）苏醒瞬间即刻执行异常状态仲裁，并立即拉高/拉低外设驱动电平。
  3. **OpenAMP 跨核通信总线**：基于共享内存（Shared Memory）和 RPMsg 虚拟总线建立高可用通信链路，与 Linux 主核实现软硬双向联动。

  ## 2. 软硬件协同拓扑

  - **运行宿主**：Core 1 (FTC310, AArch64)
  - **通信对端**：Core 0/2/3 (运行 Linux OS 与 YOLOv8 AI 引擎)
  - **物理外设映射**：
    - `GPIO2_10`：火焰传感器中断捕获（下降沿极速响应）。
    - `GPIO3_1` ：数字蜂鸣器推挽输出（DO 微秒级阻断）。

  ## 3. 构建与部署指令

  ### 3.1 交叉编译环境

  本项目需使用适用于 ARM Bare-metal 的 `aarch64-none-elf-gcc` 工具链。

  - 默认架构配置：`aarch64`
  - 支持平台：Phytium Pi (飞腾派 V3)

  ### 3.2 极速构建

  在当前 `Baremetal_Slave_Node` 目录下，执行以下命令完成固件编译：

  ```
  # 清理旧有缓存与链接文件
  make clean
  # 加载为飞腾派定制的 Aarch64 裸机配置
  make config_phytiumpi_aarch64
  # 编译并生成最终的 .elf 固件
  make image
  ```

  ### 3.3 固件下发与系统整合 (Remoteproc)

  编译成功后，将在目录下生成 `pe2204_aarch64_phytiumpi_openamp_core0.elf`（注意：由于底层兼容性，产物命名保留 core0 字样，但其真实运行域受限于 DTS 的分配，实际运行在 Core 1 上）。

  在物理板卡上，需将该固件转移至主核 Linux 侧的系统固件目录：

  ```
  # 1. 将 ELF 文件部署到系统标准路径
  scp pe2204_aarch64_phytiumpi_openamp_core0.elf root@<开发板IP>:/lib/firmware/
  
  # 2. Linux 启动后，通过 remoteproc 框架加载并唤醒异构前哨站
  echo openamp_core0.elf > /sys/class/remoteproc/remoteproc0/firmware
  echo start > /sys/class/remoteproc/remoteproc0/state
  ```

  ## 4. 通信协议规范 (RPMsg Payload)

  为防止跨指令集（Bare-metal 与 Linux）引发的 Padding 内存畸变，本系统双向通信严禁编译器自动补齐。 报文载荷强制采用 `#pragma pack(1)` 单字节对齐。

  ```
  typedef struct {
      uint32_t command;           // 4-byte 控制字 (如 0x0006U 表示火灾上报)
      uint16_t length;            // 2-byte 数据载荷长度
      char data[MAX_DATA_LENGTH]; // 可变长字符串载荷
  } ProtocolData;
  ```

  ## 5. 版权与开发申明

  本项目针对飞腾官方 OpenAMP 基础 Echo 框架进行了深度二次研发与架构重写。所有环境传感探针代码、中断响应逻辑、业务级状态机防抖机制均由团队基于飞腾 SDK 底层 API 手工编写实现。
