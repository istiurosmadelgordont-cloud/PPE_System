# 系统架构与核心业务流程图 (CICC 答辩专用)

本篇文档基于 **0.2.1 异构多核系统框架 (Linux + Bare-Metal)** 与 **0.2.2 核心业务流程**，使用 Mermaid 进行了视觉重构。同时，附带了可用于答辩 PPT 报告的扁平化系统架构图和业务流水线拓扑图设计：

### 📸 CICC 答辩标准系统架构图 (0.2.1)
![CICC 答辩系统架构图](file:///C:/Users/30406/.gemini/antigravity/brain/c0ec0f77-d185-471b-bcc1-95b86fc3c523/ppe_system_architecture_1783167215642.png)

### 📸 CICC 答辩核心业务流程图 (0.2.2)
![CICC 答辩核心业务流程图](file:///C:/Users/30406/.gemini/antigravity/brain/c0ec0f77-d185-471b-bcc1-95b86fc3c523/ppe_system_flowchart_1783167228495.png)

---

## 1. 0.2.1 飞腾 E2000Q 异构多核系统架构图 (AMP)

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#1e293b',
    'primaryTextColor': '#cbd5e1',
    'primaryBorderColor': '#334155',
    'lineColor': '#0284c7',
    'secondaryColor': '#0f172a',
    'tertiaryColor': '#1e293b',
    'mainBkg': '#0f172a',
    'nodeBorder': '#38bdf8'
  }
}}%%
graph TB
    subgraph "飞腾 E2000Q 四核物理芯片 (AMP 物理硬隔离架构)"
        %% ==================== LINUX DOMAIN ====================
        subgraph "Linux 操作系统域 (富应用多线程环境)"
            subgraph "Core 0 (小核) [GUI 渲染与轻量级 I/O 调度中枢]"
                A1["🖥️ Qt GUI 线程<br>(三色状态灯/图表显示)"]
                A2["🎥 Camera 线程<br>(V4L2 视频采集与防抖)"]
                A3["💾 IO 写盘线程<br>(原子更名安全落盘)"]
                A4["🌐 DeepSeek 线程<br>(AI顾问网络异步熔断)"]
            end
            
            subgraph "Core 2 & 3 (双大核) [AI 推理加速大脑]"
                B1["🧠 NCNN 推理线程<br>(YOLOv8 INT8 QAT 推理)"]
                B2["📊 ByteTrack 追踪引擎<br>(卡尔曼滤波轨迹关联)"]
            end
        end

        %% ==================== SHARED BUS ====================
        subgraph "跨核硬件互联总线"
            C1[("⚡ 0xB0100000 共享内存<br>(基于 OpenAMP RPMsg 环形缓冲)")]
        end

        %% ==================== BARE-METAL DOMAIN ====================
        subgraph "Core 1 (小核) [Bare-Metal 裸机安全与防灾底座]"
            D1["🔌 RPMsg 监听服务<br>(双向 500ms 心跳守护)"]
            D2["🔥 中断控制器 (EXTI)<br>(物理火焰中断优先级0秒级响应)"]
            D3["🎛️ I2C / ADC 驱动程序<br>(AHT20温湿度/MQ-2可燃气体)"]
            D4["🐕 FWDT0 硬件看门狗<br>(1s 喂狗超时硬件复位)"]
        end
    end

    %% ==================== PHYSICAL DEVICES ====================
    subgraph "物理外设与硬件实体"
        E1["📷 USB 摄像头"]
        E2["🔥 物理火焰传感器"]
        E3["💨 AHT20/MQ-2 传感器"]
        E4["📢 物理报警蜂鸣器"]
        E5["💿 EMMC / 闪存介质"]
    end

    %% 连线关系 (数据与控制流)
    E1 ==>|UVC 视频流| A2
    A2 -.->|无锁环形队列 cap_queue| B1
    B1 -->|AlarmEvent 违规载荷| B2
    B2 -->|去抖抓拍触发| A3
    A3 ===>|原子 write & rename| E5
    A1 <-->|信号槽绑定| A4

    %% 跨核连线
    A1 <-->|跨核心跳 & 外设状态控制| C1
    C1 <-->|片上硬中断中断触发| D1

    %% 裸机直控 (毫秒/微秒级低迟滞回路)
    E2 ==>|硬中断引脚| D2
    E3 ==>|I2C / ADC 采样| D3
    D2 ==>|GPIO 物理连线| E4
    D3 -->|硬检查 ERR 检测| D1
    D1 <-->|看门狗刷新控制| D4

    %% 样式定义 (Futuristic Palette)
    style A1 fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    style A2 fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    style A3 fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    style A4 fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    
    style B1 fill:#1e1b4b,stroke:#818cf8,stroke-width:2px,color:#f8fafc;
    style B2 fill:#1e1b4b,stroke:#818cf8,stroke-width:2px,color:#f8fafc;
    
    style C1 fill:#1c1917,stroke:#ca8a04,stroke-width:2px,color:#fca5a5;
    
    style D1 fill:#064e3b,stroke:#34d399,stroke-width:2px,color:#ecfdf5;
    style D2 fill:#064e3b,stroke:#34d399,stroke-width:2px,color:#ecfdf5;
    style D3 fill:#064e3b,stroke:#34d399,stroke-width:2px,color:#ecfdf5;
    style D4 fill:#7f1d1d,stroke:#f87171,stroke-width:2px,color:#fef2f2;
    
    style E1 fill:#020617,stroke:#94a3b8,stroke-width:1px,color:#e2e8f0;
    style E2 fill:#020617,stroke:#94a3b8,stroke-width:1px,color:#e2e8f0;
    style E3 fill:#020617,stroke:#94a3b8,stroke-width:1px,color:#e2e8f0;
    style E4 fill:#020617,stroke:#94a3b8,stroke-width:1px,color:#e2e8f0;
    style E5 fill:#020617,stroke:#94a3b8,stroke-width:1px,color:#e2e8f0;
```

---

## 2. 0.2.2 全系统业务流与防灾控制流程图

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#1e293b',
    'primaryTextColor': '#cbd5e1',
    'primaryBorderColor': '#475569',
    'lineColor': '#6366f1',
    'secondaryColor': '#0f172a',
    'tertiaryColor': '#1e293b',
    'mainBkg': '#0f172a',
    'nodeBorder': '#818cf8'
  }
}}%%
flowchart TD
    %% ==================== SYSTEM STARTUP ====================
    Start(["⚡ 开电自检与初始化"]) --> InitSDK["1. 从核 (Core 1) 初始化 <br> 挂载 FWDT0 硬件看门狗"]
    InitSDK --> VerifyModel{"2. 主核 (Core 0) <br> 执行 YOLOv8 模型 MD5 强校验"}
    
    VerifyModel --"❌ 完整性损坏"--> PullModel["自动调用 pull_model.sh 自愈<br> (尝试网络拉取，失败则拷贝本地备份)"]
    PullModel --> VerifyModel
    
    VerifyModel --"✅ 校验成功"--> StartInference["3. 启动双大核 AI 推理线程<br>主从核建立 OpenAMP 握手"]

    %% ==================== PIPELINE EXECUTION ====================
    subgraph "流水线数据流循环"
        StartInference --> GrabFrame["V4L2 摄像头拉流"]
        GrabFrame --> CheckCamTimeout{"是否超时 >= 3秒 未出帧?"}
        
        CheckCamTimeout --"⚠️ 是 (故障注入)"--> SwitchFallback["🚨 [Camera Fallback]<br>平滑热切换至本地 test2.mp4 视频源"]
        SwitchFallback --> ProcessInference
        
        CheckCamTimeout --"✅ 否 (正常)"--> ProcessInference["4. YOLOv8 推理 + ByteTrack 跟踪 <br> (零拷贝 const_cast 直接映射)"]
    end

    %% ==================== ALARM & MITIGATION ====================
    subgraph "声光阻断与安全落盘"
        ProcessInference --> CheckViolation{"5. 是否存在违规穿戴?"}
        
        CheckViolation --"🚨 是"--> AlarmFlow["立即执行声光阻断机制"]
        AlarmFlow --> RPMsgSendAlarm["从核直发蜂鸣器报警指令 (微秒级)"]
        AlarmFlow --> StartHoldTimer["Qt UI 报警指示灯 3 秒防抖维持"]
        AlarmFlow --> WriteSnapshot["📷 抓拍违规证据图片"]
        
        WriteSnapshot --> AtomicWrite["写入缓存文件 (*.tmp.jpg)"]
        AtomicWrite --> AtomicRename["原子更名 (std::filesystem::rename)<br>防写盘突发断电导致文件残残缺"]
        AtomicRename --> LogWrite["写入本地数据库与大屏日志"]
        
        CheckViolation --"✅ 否"--> MonitorSensors["6. 轮询温度/湿度/气体传感器"]
    end

    %% ==================== HEARTBEAT & FAULT INJECTION ====================
    subgraph "可靠性防灾与硬件自愈 (Bare-metal Watchdog)"
        MonitorSensors --> CheckHeartbeat{"7. 跨核心跳是否连续丢失?"}
        
        CheckHeartbeat --"💥 丢失 4 次 (2秒)"--> EnterFailSafe["🚨 从核停止喂狗并启动 Fail-Safe<br>强行缩短看门狗超时时间至 1 秒"]
        EnterFailSafe --> ColdReset["⚡ Watchdog 硬件溢出，整机强行冷重启自愈"]
        ColdReset --> Start
        
        CheckHeartbeat --"✅ 正常"--> CheckSensorConn{"8. 传感器物理连线是否脱落?"}
        CheckSensorConn --"⚠️ 是 (故障注入)"--> ReportErr["从核通过 RPMsg 向上发送 'ERR' 状态字"]
        ReportErr --> UIRed["Qt UI 环境参数区域变成红色，显示 '断开'"]
        
        CheckSensorConn --"✅ 否 (正常)"--> ReportNormal["从核上传实时 T / H 物理数据"]
        ReportNormal --> UpdateUI["Qt UI 界面实时刷新参数图表"]
    end

    UpdateUI --> GrabFrame
    UIRed --> GrabFrame

    %% 样式美化代码 (Modern Color Coding)
    style Start fill:#1e293b,stroke:#64748b,stroke-width:2px,color:#f8fafc;
    style PullModel fill:#7c2d12,stroke:#ea580c,stroke-width:2px,color:#ffedd5;
    style SwitchFallback fill:#7c2d12,stroke:#ea580c,stroke-width:2px,color:#ffedd5;
    style EnterFailSafe fill:#7f1d1d,stroke:#dc2626,stroke-width:2px,color:#fef2f2;
    style ColdReset fill:#991b1b,stroke:#ef4444,stroke-width:2px,color:#fee2e2;
    
    style GrabFrame fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    style ProcessInference fill:#1e1b4b,stroke:#818cf8,stroke-width:2px,color:#f8fafc;
    style RPMsgSendAlarm fill:#064e3b,stroke:#34d399,stroke-width:2px,color:#ecfdf5;
    style AtomicRename fill:#1c1917,stroke:#ca8a04,stroke-width:2px,color:#fca5a5;
    style UIRed fill:#7f1d1d,stroke:#f87171,stroke-width:2px,color:#fef2f2;
    style UpdateUI fill:#064e3b,stroke:#34d399,stroke-width:2px,color:#ecfdf5;
    
    style VerifyModel fill:#172554,stroke:#2563eb,stroke-width:1.5px,color:#eff6ff;
    style CheckCamTimeout fill:#172554,stroke:#2563eb,stroke-width:1.5px,color:#eff6ff;
    style CheckViolation fill:#172554,stroke:#2563eb,stroke-width:1.5px,color:#eff6ff;
    style CheckHeartbeat fill:#172554,stroke:#2563eb,stroke-width:1.5px,color:#eff6ff;
    style CheckSensorConn fill:#172554,stroke:#2563eb,stroke-width:1.5px,color:#eff6ff;
```
