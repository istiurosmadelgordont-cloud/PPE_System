# 📖 知识库 - 硬件配置、驱动架构

> **硬件平台**：飞腾派 E2000Q 核心板  
> **软件架构**：Linux Kernel 5.10 + Phytium Standalone SDK v3.0  
> **知识库维护**：双生序章技术团队

---

## 1. 飞腾派 J2 排针引脚电气定义与映射

在硬件连线中，需要精确对齐飞腾派 J2 排针引脚、SoC 引脚名称和 Standalone SDK 的驱动编号：

| J2 物理引脚编号 | 信号名称 (SoC Pin) | Standalone GPIO 编号 | 工作模式 | 连接传感器/外设 | 电气规范 |
| :---: | :--- | :---: | :---: | :--- | :---: |
| **Pin 1** | VCC_5V | — | Power | 系统 5V 供电输入 | 5V / 2A |
| **Pin 3** | GND | — | Power | 系统地 (Ground) | 0V |
| **Pin 4** | GPIO1_12 | `FGpioPin_12` | Input (EXTI) | 火焰传感器 (DO) | 3.3V TTL |
| **Pin 5** | GPIO1_13 | `FGpioPin_13` | Input | MQ-2 可燃气体传感器 (DO) | 3.3V TTL |
| **Pin 7** | GPIO1_15 | `FGpioPin_15` | Output | 物理报警蜂鸣器 (Buzz) | 3.3V / 10mA |
| **Pin 9** | I2C1_SDA | — | Alt Function | 温湿度 AHT20/DHT11 数据线 | I2C (上拉 4.7K) |
| **Pin 10**| I2C1_SCL | — | Alt Function | 温湿度 AHT20/DHT11 时钟线 | I2C (上拉 4.7K) |

---

## 2. Standalone SDK 核心驱动架构

从核 Baremetal 系统直接运行于 Standalone SDK 之上，其通过直接配置片上外设寄存器来实现极速驱动控制。

### 2.1 FGpio 驱动架构 (通用输入输出)
*   **句柄管理**：调用 `FGpioCfgInitialize` 初始化物理控制单元。
*   **方向控制**：使用 `FGpioSetDirection` 将引脚设置为输入（`FGPIO_DIR_INPUT`）或输出（`FGPIO_DIR_OUTPUT`）。
*   **引脚读写**：
    ```c
    // 向蜂鸣器引脚写入高电平以启动报警
    FGpioPinWrite(&gpio_ctrl, FGpioPin_15, FGPIO_PIN_HIGH);
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
*   **刷新寄存器**：通过向 `FWDT_CRR`（刷新寄存器）写入特定键值（`0x76`）完成喂狗，避免向复位管理器发送 CPU 物理复位请求。

---

## 3. Linux Kernel `remoteproc` 异构驱动框架

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

*   **`phytium_rproc` 驱动**：负责从核固件的 ELF 解析、内存分配、中断注册及固件写入操作。通过 `/sys/class/remoteproc/remoteproc0/` 向用户空间提供 `state`（控制从核状态）和 `firmware`（控制加载固件名）两个物理接口。
*   **`virtio_rpmsg_bus` 驱动**：在 remoteproc 启动后，建立基于 VirtIO 协议的环形队列，将共享内存虚拟为标准网络/串口设备。
*   **`rpmsg_char` 驱动**：将远程信道包装为 `/dev/rpmsg0` 字符设备节点，用户层程序直接通过标准的 `open()`、`read()`、`write()` 操作即可执行高实时数据交互。

---

## 4. ARM NEON 指令集向量化加速原理

Cortex-A72 大核具备高性能单指令多数据（SIMD）扩展——ARM NEON 向量协处理器。本系统的 AI 推理模块（NCNN）通过以下机制实现推理的硬件级加速：

*   **128位寄存器打包**：NEON 拥有 32 个 128 位寄存器。可以将 16 个 8 位整型数（INT8）或 4 个 32 位浮点数（FP32）一次性打包进单个寄存器内。
*   **硬件点积乘加 (SDOT/UDOT)**：支持单条指令完成两个向量的 4 元素 dot-product 乘加，是加速卷积神经网络中矩阵乘法（General Matrix Multiplication, GEMM）的终极物理利器。
*   **内存打包布局 (Packed Layout)**：在 [inference_node.cpp](file:///d:/飞腾派/CICC1004607+初赛+技术数据(代码类)/ppe4-28/Phytium_PPE_System_SourceCode/ppe_system/src/inference_node.cpp) 中配置 `opt.use_int8_packed = true`。这使得 NCNN 矩阵内存布局重排为连续的 8 字节对齐，完美匹配 NEON 的批量内存加载指令（`vld1`），消除了 CPU 寻址多余的开销。
