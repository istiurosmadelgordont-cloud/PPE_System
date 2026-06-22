# 📖 操作手册 — 如何运行、测试、验证、绘图 (自包含)

> **项目名称**：基于飞腾派 E2000Q 的异构多核 PPE 智能监控系统  
> **团队名称**：双生序章  
> **适用平台**：飞腾派 E2000Q (FTC664 1+2+1 异构多核处理器)

---

## 1. 编译前置依赖与开发环境配置

本系统分为 **Linux 主核系统域** (Cortex-A72 Core 0, 2, 3) 与 **Baremetal 从核裸机域** (Cortex-A72 Core 1)，需要分别配置对应的编译链与开发库。

### 1.1 Linux 主核开发环境 (AArch64)
在飞腾派本地或 AArch64 交叉编译容器中，执行以下命令安装必要依赖：
```bash
# 更新源并安装基础编译套件
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config

# 安装 Qt5 开发库 (用于系统监控大屏与可视化 UI)
sudo apt-get install -y qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev libqt5charts5-dev

# 安装 OpenCV 4.x (用于摄像头拉流与图片编码)
sudo apt-get install -y libopencv-dev

# NCNN 推理引擎依赖
sudo apt-get install -y libvulkan-dev libprotobuf-dev protobuf-compiler
```

### 1.2 从核 Standalone 裸机编译链
从核 Baremetal 的编译在 Windows/Linux 主机上使用 `arm-none-eabi-gcc` 交叉编译器进行：
1. 下载 GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`，建议版本 `9-2020-q2-update` 或更高)。
2. 将工具链的 `bin` 目录添加至系统 `PATH` 环境变量中。
3. 验证编译器是否可用：
   ```bash
   arm-none-eabi-gcc --version
   ```

---

## 2. 从核 Baremetal 固件编译与加载部署

从核固件直接运行于 Core 1，不依赖操作系统调度。

### 2.1 编译从核固件
在主机或开发板上进入从核源码目录 `Baremetal_Slave_Node`：
```bash
cd Baremetal_Slave_Node
# 清理并重新生成固件
make clean
make -j4
```
编译成功后，在当前目录下会生成 ELF 格式的固件文件：`pe2204_aarch64_phytiumpi_openamp_core0.elf`。

### 2.2 加载部署至 remoteproc 核心管理器
在飞腾派 Linux 主核上，执行以下步骤将固件写入固件库并唤醒 Core 1：
```bash
# 1. 复制从核固件至 Linux 系统固件目录
sudo cp pe2204_aarch64_phytiumpi_openamp_core0.elf /lib/firmware/

# 2. 软解绑并重新加载 remoteproc 驱动（如有必要）
echo stop | sudo tee /sys/class/remoteproc/remoteproc0/state || true

# 3. 指定 remoteproc 加载的固件名称
echo pe2204_aarch64_phytiumpi_openamp_core0.elf | sudo tee /sys/class/remoteproc/remoteproc0/firmware

# 4. 唤醒 Core 1 启动运行从核裸机系统
echo start | sudo tee /sys/class/remoteproc/remoteproc0/state
```
**验证启动状态**：
通过以下命令查看从核的实时串口/调试输出：
```bash
sudo cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
```
如看到 `[SLAVE_00] remoteproc init successfully` 以及看门狗初始化成功的日志，即说明从核已成功运行。

---

## 3. 主核 Linux 应用程序编译与运行

### 3.1 编译主核程序
在飞腾派上进入 `ppe_system` 目录：
```bash
cd ppe_system
mkdir -p build && cd build
cmake ..
make -j4
```
编译完成后，会在 `build` 目录下生成 `ppe_system` 可执行文件。

### 3.2 启动主核程序
#### 3.2.1 完整图形界面模式 (GUI)
开发板已连接 HDMI 屏幕时，在终端中启动：
```bash
export DISPLAY=:0
sudo -E ./ppe_system
```

#### 3.2.2 离线/无头测试模式 (Offscreen)
在无显示器的 SSH 远程调试或运行自动化测试脚本时，使用 Qt 离线渲染模式：
```bash
sudo ./ppe_system -platform offscreen
```

---

## 4. 系统测试与验证说明 (TC-01 ~ TC-14)

为验证本系统的 14 项核心需求 (REQ-01 ~ REQ-14)，可在 `ppe_system/build` 中运行程序，并按照如下步骤进行测试：

### 4.1 基础业务与 AI 推理测试 (TC-01 ~ TC-05)
1. **USB 摄像头实时拉流与绑核 (TC-01)**：启动程序后，画面展示摄像头实时画面，使用 `htop` 观察 Core 0，其负载将有轻微上升，说明拉流线程成功绑定小核。
2. **录像导入与热切换 (TC-02)**：在界面点击“导入录像”加载测试视频。视频播放完毕后，验证系统在 1 秒内无缝切回摄像头实时流。
3. **多线程并发稳定性 (TC-03)**：保持系统连续运行 4 小时以上，验证无死锁、无 Segfault 发生。
4. **PPE 八类目标检测与遮挡追踪 (TC-04, TC-05)**：工装穿戴人员在镜头前移动并作交叉走动，验证界面上的边界框及 ByteTrack 追踪 ID 稳定，不受短暂遮挡影响。

### 4.2 物理联动与中断测试 (TC-06, TC-07)
1. **防灾环境上报 (TC-07)**：从核每隔一定周期通过 RPMsg 总线向主核上报 AHT20 温湿度。验证主核 Qt 界面看板上的温度、湿度曲线实时变化。
2. **有害气体/火焰报警物理阻断 (TC-06)**：
   * 在传感器端注入数字低电平（火焰或可燃气体告警）。
   * 从核的 GPIO 硬件中断（EXTI 中断优先级 0）瞬间捕获电平跳变，并在 1ms 内拉高物理蜂鸣器阻断，同时向主核发送 RPMsg 帧。
   * 验证主核 Qt UI 瞬间弹出红色报警字样。

---

## 5. 关键安全机制与故障注入测试 (FIT) 验证

故障注入测试 (Fault Injection Test, FIT) 旨在验证系统在遭遇极端异常时的自愈与容错能力。

### 5.1 【主控跑飞/死锁】故障注入 (TC-12, TC-13)
* **注入步骤**：
  1. 启动 `ppe_system` 程序，确认主从核心跳正常（Qt 界面“RPMsg”徽章为绿色正常状态）。
  2. 在终端发送信号强行挂起主核进程，模拟主进程发生死锁或挂起：
     ```bash
     sudo kill -STOP $(pidof ppe_system)
     ```
  3. **观察从核接管行为**：由于主核被挂起，发送心跳包（500ms 周期）的线程停止工作。2秒后，从核检测到心跳丢失（miss >= 4），进入 Fail-Safe 安全接管状态：从核本地的蜂鸣器开始持续鸣叫报警，并且从核停止对硬件看门狗 `FWDT0` 喂狗。
  4. **观察硬件冷重启**：看门狗定时器在停止喂狗 10s 后超时，飞腾派 SoC 触发物理 cold reset，整机重新启动。

### 5.2 【RPMsg 数据篡改】故障注入 (TC-11)
* **注入步骤**：
  1. 主核向从核的控制节点 `/dev/rpmsg0` 强行写入未经过 CRC8 校验或篡改了校验码的非法数据帧。
  2. 观察从核的调试终端（`trace0`）：从核打印 `[CRC] Frame verification failed!` 日志，并且丢弃该控制指令，蜂鸣器状态保持稳定，未发生越权控制。

### 5.3 【断电数据保存失败】故障注入 (TC-10)
* **注入步骤**：
  1. 系统运行且不断触发违规抓拍存图，向 `/tmp` 或 `/home/user/violations_data` 写入抓拍。
  2. 使用如下命令强行终止 I/O 线程，或直接切断开发板电源：
     ```bash
     sudo kill -9 $(pidof ppe_system)
     ```
  3. **验证原子写入**：重新开机后，检查抓拍文件夹，验证所有图片文件均为完好图片，没有任何一张 0 字节或头部损坏的残缺图片（因为写入是先写到 `.tmp`，再执行瞬间的 `rename()` 原子重命名操作）。

---

## 6. 性能数据采集与图表绘制 (Python)

为配合集创赛决赛答辩的 PPT 报告，本系统提供了自包含的性能指标评估与曲线自动绘制脚本。

### 6.1 启动性能监控与日志导出
主核程序运行时，后台会自动统计以下三项指标并写入当前目录的 `performance_metrics.csv`：
* NCNN AI 推理延迟（毫秒/帧）
* OpenAMP RPMsg 双向通信时延（微秒）
* CPU 整体及单核占用率（%）

### 6.2 运行绘图脚本
在 `temp_helper` 目录下，运行绘图脚本（一律在 D 盘工作目录下运行）：
```powershell
# 在 Windows 开发宿主机上，进入 D 盘项目目录
cd D:\飞腾派\CICC1004607+初赛+技术数据(代码类)\ppe4-28\Phytium_PPE_System_SourceCode\temp_helper
python .\read_trace_now.py
```
该脚本将解析导出的性能数据，并在 `temp_helper/` 生成三张高清图表：
1. `ncnn_fps_acceleration.png`：单核 vs 多核（OpenMP + NEON 加速）帧率对比柱状图。
2. `rpmsg_latency_distribution.png`：跨核 OpenAMP 通信时延分布箱线图。
3. `cpu_load_balancing.png`：Core 0~3 的负载均衡折线图。

评委可直接利用生成的图表对系统的极限加速性能与实时性指标进行直观核对。
