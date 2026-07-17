# 硬件接口与异构运行环境参考

> **硬件平台**：飞腾派 E2000Q 核心板  
> **软件架构**：Linux Kernel 5.10 + Phytium Standalone SDK v3.0  
> **知识库维护**：双生序章技术团队

---

## 1. 已验证硬件接口表

在硬件连线中，需要精确对齐飞腾派排针引脚、SoC 引脚名称和 Standalone SDK 的驱动编号：

| 物理引脚编号 | 信号名称 (SoC Pin) | Standalone GPIO 编号 | 工作模式 | 连接传感器/外设 | 电气规范 |
| :---: | :--- | :---: | :---: | :--- | :---: |
| **Pin 7** | GPIO2_10 | `FGpioPin_10` | Input (EXTI) | 火焰传感器 (DO) | 3.3V TTL |
| **Pin 13**| GPIO1_12 | `FGpioPin_12` | Input | MQ-2 可燃气体传感器 (DO) | 3.3V TTL |
| **Pin 3** | I2C1_SDA | — | MIO1 Alt | 温湿度 AHT20 数据线 | I2C (上拉 4.7K) |
| **Pin 5** | I2C1_SCL | — | MIO1 Alt | 温湿度 AHT20 时钟线 | I2C (上拉 4.7K) |
| **[实际引脚]** | GPIO1_15 | `FGpioPin_15` | Output | 物理报警蜂鸣器 (Buzz) | 3.3V / 10mA |

---

## 2. 从核 Standalone SDK 驱动

从核 Baremetal 系统直接运行于 Standalone SDK 之上，其通过直接配置片上外设寄存器来实现极速驱动控制。

### 2.1 FGpio 驱动架构 (通用输入输出)
*   **句柄管理**：调用 `FGpioCfgInitialize` 初始化物理控制单元。
*   **方向控制**：使用 `FGpioSetDirection` 将引脚设置为输入（`FGPIO_DIR_INPUT`）或输出（`FGPIO_DIR_OUTPUT`）。
*   **引脚读写**：
    ```c
    // 向蜂鸣器引脚写入低电平以启动报警（低有效）
    FGpioPinWrite(&gpio_ctrl, FGpioPin_15, FGPIO_PIN_LOW);
    // 读取可燃气体传感器的电平状态
    u32 value = FGpioPinRead(&gpio_ctrl, FGpioPin_13);
    ```

### 2.2 FI2c 驱动架构 (物理 I2C 总线控制)
温湿度传感器 AHT20 挂载于物理 `I2C1` 控制器。
*   **控制器初始化**：`FI2cCfgInitialize` 配置总线工作于 Master 模式。
*   **速率配置**：时钟主频设定为 `F_I2C_SPEED_STANDARD` (100 KHz)。
*   **时序读写**：
    *   通过 `FI2cMasterWrite` 向 AHT20 写入测量触发指令。
    *   通过 `FI2cMasterRead` 读取 6 字节测量数据。

### 2.3 FWdt 驱动架构 (片上物理看门狗)
飞腾 E2000Q 芯片集成了独立的硬件看门狗控制器 `FWDT0`。
*   **时钟源**：挂载于系统总线时钟 (APB)，拥有独立的硬件倒计时器。
*   **刷新机制**：通过调用 SDK 接口进行喂狗，避免看门狗超时向复位管理器发送硬件复位请求。

---

## 3. Linux 侧异构通信环境

在 Linux 主核侧，从核被抽象为一个远程处理器设备，其生命周期和通信链路完全由内核驱动管理：

```
                    ┌─────────────────────────┐
                    │      用户空间应用        │
                    │      (ppe_system)       │
                    └───────────┬─────────────┘
                                │ 读写控制
                                ▼
                     [ Linux 虚拟文件系统 VFS ]
                                │
   ┌────────────────────────────┼────────────────────────────┐
   │ 内核空间驱动               ▼                            │
   │                ┌───────────────────────┐                │
   │                │   rpmsg_char / rpmsgX │                │
   │                └───────────┬───────────┘                │
   │                            ▼                            │
   │                ┌───────────────────────┐                │
   │                │   virtio_rpmsg_bus    │                │
   │                └───────────┬───────────┘                │
   │                            ▼                            │
   │                ┌───────────────────────┐                │
   │                │    phytium_rproc      │                │
   │                └───────────────────────┘                │
   └─────────────────────────────────────────────────────────┘
```

*   **`phytium_rproc` 驱动**：remoteproc 框架负责从核固件的加载、启动和生命周期管理。通过 `/sys/class/remoteproc/remoteproc0/` 向用户空间提供控制接口。
*   **`virtio_rpmsg_bus` 驱动**：在 remoteproc 启动后，建立基于 VirtIO 协议的环形队列，将共享内存虚拟为标准网络/串口设备。
*   **`rpmsg_char` 驱动**：将远程信道包装为字符设备节点。应用层程序会在运行时动态扫描发现 `/dev/rpmsg[0-9]*` 数据端点，直接通过标准的 `open()`、`read()`、`write()` 操作执行高实时数据交互。

---

## 4. AI 推理优化事实

飞腾派 E2000Q 多核平台具备高性能单指令多数据（SIMD）扩展。本系统的 AI 推理模块（NCNN）通过以下机制实现推理的硬件级加速：

NCNN 推理模块启用 INT8 推理、INT8 packed 数据布局及 INT8 存储选项，并将推理任务定向运行于 Core 2/3，以降低与视频采集、UI 和存储任务的资源竞争。

*   **INT8 推理与存储**：配置 `opt.use_int8_inference = true` 和 `opt.use_int8_storage = true`，大幅降低内存占用与带宽消耗。
*   **内存打包布局 (Packed Layout)**：配置 `opt.use_int8_packed = true`。这使得 NCNN 矩阵内存布局重排为连续对齐，提升内存加载指令效率，消除 CPU 寻址多余开销。
