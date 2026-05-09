# 系统功能源代码与验证测试包

## 1. 软硬件工作平台申明



### 1.1 硬件平台

- **主控板卡**：[ PhytiumPiv3]
- **处理器架构**：异构多核 ARMv8 + Cortex-M
- **外设清单**：HDMI 显示器、USB 摄像头、蜂鸣器等

### 1.2 软件与系统环境

- **操作系统**：[phytiumpi-os-v2.1]
- **实时操作系统(从核)**：[裸机]
- **核心依赖库**：
  - Qt 5.15.2 (GUI界面渲染)
  - OpenCV 4.x (图像处理)
  - NCNN (Tencent 高性能神经网络前向计算框架)
  - Eigen3 (矩阵运算库)
  - OpenAMP / RPMsg (跨核通信)
- **编译工具链**：GCC / CMake

## 2. 工程代码结构与设计报告对应关系

本压缩包内的代码结构已与《项目详细设计报告》严格对齐，核心模块映射如下：

```
📦ppe_system
 ┣ 📂 build/                   # 编译输出目录及可执行文件所在位置
 ┣ 📂 include/                 # 头文件目录
 ┣ 📂 model/                   # AI 引擎调用的模型权重文件 (NCNN/YOLO格式)
 ┣ 📂 src/                     # 核心业务逻辑源码
 ┃ ┣ 📂 ByteTrack/             # 【多目标追踪算法】对应报告中连续视频帧的目标重识别
 ┃ ┣ 📜 main.cpp               # 【主程序入口】对应报告系统总调度
 ┃ ┣ 📜 ui_main_window.cpp     # 【界面交互模块】对应报告零拷贝渲染架构
 ┃ ┣ 📜 inference_node.cpp     # 【AI 推理模块】对应报告目标检测与违规判定
 ┃ ┣ 📜 camera_node.cpp        # 【视频拉流模块】负责外设摄像头与本地视频的解码抽帧
 ┃ ┣ 📜 io_node.cpp            # 【数据落盘模块】对应报告抓拍证据保存
 ┃ ┣ 📜 rpmsg_node.cpp         # 【底层硬件联动模块】对应报告跨核通信与防抖设计
 ┃ ┗ 📜 global_context.cpp     # 【全局上下文】跨线程状态与变量同步管理
 ┣ 📂 video/                   # 测试视频流/本地回放文件目录
 ┣ 📂 violations_data/         # 违规抓拍图片证据存储目录
 ┣ 📜 pe2204_aarch64...elf     # 从核 RTOS 固件(必需，用于跨核硬件联动)
 ┣ 📜 CMakeLists.txt           # 工程主编译脚本
 ┣ 📜 setup_env.sh             # 环境变量配置与辅助启动脚本
 ┣ 📜 README.md                # 本交付物说明与测试指南
 ┗ 📜 CHANGELOG.md             # 研发版本迭代履历
```

## 3. 核心代码履历 (Changelog)

本系统在研发周期内经历了四次重大架构演进，充分压榨了主控板卡的底层硬件潜力：

- **[v2.0.0] - 最终交付版：异构多核物理隔离与硬件级联动**
  - **架构重构**：彻底打破单系统局限，实现异构多核协同。将 Linux 部署于 3 个高性能核心处理 AI 与 UI 渲染；将 1 个小核剥离运行裸机（Bare Metal）/ RTOS 固件。
  - **通信机制**：引入 OpenAMP/RPMsg 框架实现跨核通信，主核识别违规后发送毫秒级中断指令至小核，由小核直接控制底层蜂鸣器等外设，保障了物理警报的绝对实时性与安全性。
- **[v1.5.0] - 性能优化版：多核并行与异步流水线**
  - **并发调度**：引入 `std::thread` 与 `std::mutex` 重构系统调度，将视频解码（拉流）、AI 深度学习推理、目标追踪（ByteTrack）与图像落盘分布至不同的 CPU 核心并行处理，大幅提升系统吞吐率（FPS）。
  - **异常处理**：增加文件 IO 防抖与线程安全队列，防止高并发下的数据竞争。
- **[v1.2.0] - 交互升级版：Qt GUI 零拷贝渲染接入**
  - **界面集成**：摒弃简陋的命令行输出，完整接入 Qt5 图形化界面系统，实现监控画面与报警日志的实时可视化。
  - **显存优化**：基于 Qt 信号槽系统与 OpenCV 智能指针，实现 AI 推理线程向 UI 渲染线程的数据“零拷贝（Zero-Copy）”传递，极大降低了系统内存带宽压力。
- **[v1.0.0] - 原型验证版：INT8 模型量化与基础推理打通**
  - **算法部署**：完成基础交叉编译环境搭建，成功基于 NCNN 框架拉起 YOLO 模型。
  - **推理加速**：全面应用 INT8 精度量化技术，在保证识别精度损失极小的前提下，将神经网络前向计算速度提升数倍，为后续多模块并行奠定算力基础。

## 4. 可执行文件编译与测试执行步骤 (必看)

*(评审要求 2：可执行文件，附必要的执行测试步骤)*

为了保证系统在测试平台的稳定运行，请测试人员务必按照以下流程进行环境配置与编译运行。

### 4.1 系统依赖与 NCNN 框架安装

打开终端，更新软件源并安装基础依赖：

```
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libopencv-dev libeigen3-dev -y
```

获取并编译 NCNN 源码（推荐使用国内镜像加速）：

```
git clone [https://gitee.com/mirrors/ncnn.git](https://gitee.com/mirrors/ncnn.git)
cd ncnn
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNCNN_VULKAN=OFF -DNCNN_BUILD_EXAMPLES=OFF ..
make -j4
sudo make install
```

### 4.2  核心配置：工作路径适配 (极其重要)

由于本系统涉及严格的本地图像抓拍证据链固化，需确保写入路径的系统权限。在编译主程序前，**请务必根据当前测试机的用户名，修改系统中的绝对路径**。

1. **修改 IO 节点路径**： 打开 `src/io_node.cpp`，找到以下变量并修改为你当前系统的真实绝对路径：

   ```
   std::string base_dir = "/home/【修改为您的当前用户名】/ppe_system/violations_data";
   ```

2. **修改推理节点路径**： 打开 `src/inference_node.cpp`，找到并修改期望输出路径（与上述路径保持一致）：

   ```
   std::string expected_img_path = "/home/【修改为您的当前用户名】/ppe_system/violations_data/" + std::string(time_str_file) + "_" + v_name + ".jpg";
   ```

### 4.3 源码编译

配置完成后，进入本工程根目录进行编译：

```
cd ~/ppe_system  # 请替换为实际解压目录
mkdir -p build && cd build
cmake ..
make -j4
```

### 4.4  前置条件：从核固件加载与启动

**注意：** 本系统基于异构多核架构。在启动主核 AI 程序前，**必须**先启动从核（Cortex-M）固件，否则底层硬件通信节点将无法建立！

请按以下步骤操作（基于标准 Linux remoteproc 框架）：

1. **将根目录的固件部署至系统目录**：

   ```
   # 为方便操作，拷贝时重命名为 openamp_core0.elf
   sudo cp ~/ppe_system/pe2204_aarch64_phytiumpi_openamp_core0.elf /lib/firmware/openamp_core0.elf
   ```

2. **启停从核**：

   ```
   # 1. 停止可能正在占用资源的旧进程
   echo stop | sudo tee /sys/class/remoteproc/remoteproc0/state
   
   # 2. 绑定新固件
   echo openamp_core0.elf | sudo tee /sys/class/remoteproc/remoteproc0/firmware
   
   # 3. 正式启动从核
   echo start | sudo tee /sys/class/remoteproc/remoteproc0/state
   ```

*(注：如果您使用的是主板官方提供的一键启停辅助脚本 `setup_env.sh`，也可直接执行该脚本拉起从核。)*

### 4.5 权限配置与运行测试

**准备工作：** 请确保测试主板已连接 **HDMI 显示器** 和 **USB 摄像头**，并且**已确认 4.4 步骤中的从核已成功运行**。

进入 `build` 文件夹，执行以下指令启动系统：

```
cd build

# 1. 赋予跨核硬件通信节点读写权限 (仅在从核启动后才会出现这些节点)
sudo chmod 666 /dev/rpmsg_ctrl0
sudo chmod 666 /dev/rpmsg0

# 2. 配置 GUI 图形化显示权限
export DISPLAY=:0     # 指定画面输出到主显示器
xhost +               # 允许 root 权限下的 Qt 程序访问 X11 Server 

# 3. 携带显示器环境变量，以最高权限启动监控系统！
sudo -E ./ppe_system
```

### 4.6 功能验证步骤

1. **画面与 UI 渲染验证**：程序启动后，左侧视频监控区应正常渲染，右侧状态栏正常显示。
2. **硬件监控验证**：观察顶部状态栏，确认“温度”与“CPU使用率”每隔2秒动态刷新。
3. **AI 违规抓拍验证**：当摄像头画面中出现预设的违规行为时，右侧日志表格会实时新增记录。同时，进入 `violations_data/` 目录，验证抓拍图片是否成功落盘（此步骤验证 4.2 节配置是否生效）。
4. **跨核警报验证**：在发生违规时，同步观察界面“物理警报(RPMsg)”指示状态。同时，可听到外接的**蜂鸣器发出真实警报响声**，此步骤验证了主核向从核发送中断指令的物理链路连通性。