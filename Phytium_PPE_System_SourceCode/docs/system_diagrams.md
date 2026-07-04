# 系统架构图与核心业务流程图 (CICC 答辩专用)

本篇文档基于 **0.2.1 异构多核系统框架 (Linux + Bare-Metal)** 与 **0.2.2 核心业务流程 (视频拉取 -> AI 推理 -> 声光阻断 -> 证据落盘)**，结合飞腾派 E2000Q 物理硬件映射，绘制了高可视化的系统架构图和业务流程图。可作为技术报告插图或 PPT 展示。

---

## 1. 0.2.1 飞腾 E2000Q 异构多核系统架构图 (AMP)

本系统采用 **AMP (非对称多处理) 架构**，对飞腾 E2000Q 的 4 个物理核心进行了硬隔离划分：
- **Core 0 (小核)**：运行 Linux OS，负责轻量级 I/O、X11 GUI 渲染与 HTTP 网络通信。
- **Core 1 (小核)**：运行 **Bare-Metal (裸机防灾底座)**，接管硬件中断与物理看门狗。
- **Core 2 & 3 (大核)**：运行 Linux 主推理进程，通过核心亲和性绑定，强行压榨双大核进行 YOLOv8 矩阵乘法加速。
- **双核通道**：通过设备树划定的 `0xB0100000` 物理片上共享内存，实现基于 OpenAMP RPMsg 的跨核可信通信总线。

```mermaid
graph TB
    subgraph "飞腾 E2000Q 四核物理芯片"
        %% ==================== LINUX DOMAIN ====================
        subgraph "Linux 操作系统域 (富容器环境)"
            subgraph "Core 0 (小核) [GUI & I/O 调度中枢]"
                A1["Qt GUI 线程<br>(三色状态灯/图表显示)"]
                A2["Camera 线程<br>(V4L2 / USB UVC 采集)"]
                A3["IO 写盘线程<br>(原子更名落盘)"]
                A4["DeepSeek 线程<br>(网络异步熔断机制)"]
            end
            
            subgraph "Core 2 & 3 (双大核) [AI 推理大脑]"
                B1["NCNN 推理线程<br>(YOLOv8 INT8 QAT)"]
                B2["ByteTrack 追踪引擎<br>(卡尔曼滤波轨迹跟踪)"]
            end
        end

        %% ==================== SHARED BUS ====================
        subgraph "片上物理总线"
            C1[("0xB0100000 共享内存<br>(OpenAMP RPMsg 环形缓冲)")]
        end

        %% ==================== BARE-METAL DOMAIN ====================
        subgraph "Core 1 (小核) [Bare-Metal 裸机防灾底座]"
            D1["RPMsg 监听服务<br>(双向 500ms 心跳守护)"]
            D2["中断控制器 (EXTI)<br>(火焰中断优先级 0 抢占)"]
            D3["ADC / I2C 硬件驱动器<br>(AHT20 温湿度/MQ-2 可燃气)"]
            D4["FWDT0 硬件看门狗<br>(1s 硬件超时冷重启)"]
        end
    end

    %% ==================== PHYSICAL DEVICES ====================
    subgraph "外设与硬件实体"
        E1["USB 摄像头"]
        E2["物理双色火焰探头"]
        E3["AHT20 / MQ-2 传感器"]
        E4["物理报警蜂鸣器"]
        E5["EMMC / TF 存储介质"]
    end

    %% 连线关系
    E1 ==>|V4L2 视频流| A2
    A2 -->|LockFreeQueue| B1
    B1 -->|AlarmEvent| A3
    A3 ==>|原子 rename| E5
    A1 <-->|信号槽关联| A4

    %% 跨核连线
    A1 <-->|RPMsg 控制协议| C1
    C1 <-->|RPMsg 状态上报| D1

    %% 裸机直控
    E2 ==>|物理中断| D2
    E3 ==>|I2C / ADC| D3
    D2 ==>|微秒级直控| E4
    D3 -->|断线 ERR 检测| D1
    D1 <-->|喂狗/超时控制| D4

    classDef core0 fill:#e1f5fe,stroke:#01579b,stroke-width:2px;
    classDef core23 fill:#efebe9,stroke:#4e342e,stroke-width:2px;
    classDef shared fill:#fff9c4,stroke:#fbc02d,stroke-width:2px;
    classDef baremetal fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px;
    classDef hw fill:#eceff1,stroke:#37474f,stroke-width:1px;

    class A1,A2,A3,A4 core0;
    class B1,B2 core23;
    class C1 shared;
    class D1,D2,D3,D4 baremetal;
    class E1,E2,E3,E4,E5 hw;
```

---

## 2. 0.2.2 全系统业务流与防灾控制流程图

该图展现了从 **图像采集 -> AI 推理与去抖 -> 异常判定与硬件级声光阻断 -> 证据落盘** 的端到端时序与控制防线：
- **防线 1 (自愈防线)**：V4L2 摄像头超时 $3\text{s}$ 自动降级本地 MP4；
- **防线 2 (校验防线)**：YOLOv8 模型启动 MD5 校验，失败则触发 `pull_model.sh` 远程/本地双重自愈恢复；
- **防线 3 (外设防线)**：从核断线发送 `"ERR"`，UI 强制变红显示 `"断开"`；
- **防线 4 (可靠防线)**：心跳连续 $4$ 次丢失（$2\text{s}$），从核缩短看门狗至 $1\text{s}$ 并不喂狗，冷重启整机。

```mermaid
flowchart TD
    %% ==================== SYSTEM STARTUP ====================
    Start(["⚡ 系统启动 (电源开启)"]) --> InitSDK["1. 从核 (Core 1) 初始化 <br> 挂载 FWDT0 硬件看门狗"]
    InitSDK --> VerifyModel{"2. 主核 (Core 0) <br> 进行 YOLOv8 模型 MD5 校验"}
    
    VerifyModel --"校验失败"--> PullModel["自动调用 pull_model.sh 自愈<br> (尝试网络拉取，失败则拷贝本地备份)"]
    PullModel --> VerifyModel
    
    VerifyModel --"校验通过"--> StartInference["3. 启动双大核 AI 推理线程<br>主从核握手，开启 500ms 心跳"]

    %% ==================== PIPELINE EXECUTION ====================
    subgraph "视频推理主循环"
        StartInference --> GrabFrame["V4L2 采集视频帧"]
        GrabFrame --> CheckCamTimeout{"是否超时 >= 3秒 未出帧?"}
        
        CheckCamTimeout --"是 (故障注入)"--> SwitchFallback["🚨 [Camera Fallback]<br>切换至本地 test2.mp4 视频流"]
        SwitchFallback --> ProcessInference
        
        CheckCamTimeout --"否 (正常)"--> ProcessInference["4. YOLOv8 推理 + ByteTrack 跟踪 <br> (零拷贝 const_cast 直接映射)"]
    end

    %% ==================== ALARM & MITIGATION ====================
    subgraph "声光阻断与落盘判定"
        ProcessInference --> CheckViolation{"5. 检测到违规穿戴?<br>(Without Helmet/Vest)"}
        
        CheckViolation --"是"--> AlarmFlow["🚨 触发声光阻断流"]
        AlarmFlow --> RPMsgSendAlarm["从核直发蜂鸣器报警指令 (μs级)"]
        AlarmFlow --> StartHoldTimer["Qt UI 锁定报警指示灯 3 秒防抖动"]
        AlarmFlow --> WriteSnapshot["📷 抓拍违规图片证据"]
        
        WriteSnapshot --> AtomicWrite["写入缓存文件 (*.tmp.jpg)"]
        AtomicWrite --> AtomicRename["原子更名 (std::filesystem::rename)<br>防止写盘瞬间断电损坏"]
        AtomicRename --> LogWrite["记录 SQL / 文本日志并更新大屏"]
        
        CheckViolation --"否"--> MonitorSensors["6. 轮询物理传感器数据"]
    end

    %% ==================== HEARTBEAT & FAULT INJECTION ====================
    subgraph "安全守护与硬件自愈 (Bare-metal watchdog)"
        MonitorSensors --> CheckHeartbeat{"7. 主从核心跳是否丢失?"}
        
        CheckHeartbeat --"连续 4 次丢失 (2秒)"--> EnterFailSafe["🚨 从核断开喂狗并启动 Fail-Safe <br> 强制将 FWDT0 狗超时设为 1s"]
        EnterFailSafe --> ColdReset["⚡ Watchdog 硬件溢出，整机冷重启自愈"]
        ColdReset --> Start
        
        CheckHeartbeat --"正常"--> CheckSensorConn{"8. 传感器硬件接线是否断开?"}
        CheckSensorConn --"是 (故障注入)"--> ReportErr["从核发送 'ERR' 状态码"]
        ReportErr --> UIRed["Qt UI 界面温度/湿度爆红显示 '断开'"]
        
        CheckSensorConn --"否 (正常)"--> ReportNormal["从核上报真实 T / H 数值"]
        ReportNormal --> UpdateUI["Qt UI 实时更新温湿度曲线"]
    end

    UpdateUI --> GrabFrame
    UIRed --> GrabFrame

    classDef process fill:#eceff1,stroke:#37474f,stroke-width:1px;
    classDef decision fill:#fff9c4,stroke:#fbc02d,stroke-width:1.5px;
    classDef recovery fill:#ffe0b2,stroke:#fb8c00,stroke-width:2px;
    classDef critical fill:#ffebee,stroke:#c62828,stroke-width:2px;

    class InitSDK,StartInference,GrabFrame,ProcessInference,RPMsgSendAlarm,StartHoldTimer,WriteSnapshot,AtomicWrite,AtomicRename,LogWrite,ReportErr,ReportNormal,UpdateUI,UIRed process;
    class VerifyModel,CheckCamTimeout,CheckViolation,CheckHeartbeat,CheckSensorConn decision;
    class PullModel,SwitchFallback,EnterFailSafe recovery;
    class ColdReset critical;
```
