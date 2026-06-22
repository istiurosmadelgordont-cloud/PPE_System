# 📖 部署指南 — 编译工具链、环境变量与自动部署

> **目标平台**：飞腾派 E2000Q Aarch64 嵌入式 Linux  
> **宿主编译主机**：Ubuntu 20.04 LTS / Windows 10/11 x64  
> **工具链**：AArch64 GNU 工具链 + ARM None EABI 工具链

---

## 1. 编译工具链配置

### 1.1 裸机从核交叉编译器 (arm-none-eabi-gcc)
从核 Standalone 固件使用标准 GCC 裸机交叉编译器进行构建。
1.  **工具链获取**：建议下载 `gcc-arm-none-eabi-10.3-2021.10` 官方包。
2.  **环境变量配置**：
    *   在 Linux 编译机上：
        ```bash
        export PATH=/opt/gcc-arm-none-eabi/bin:$PATH
        ```
    *   在 Windows 编译机上：将 `bin` 路径配置入系统高级环境变量 `Path` 中。
3.  **开发包 SDK 挂载**：在 `Baremetal_Slave_Node` 中，默认集成了 `phytium-standalone-sdk-master` 作为底层支持，无需额外下载。

### 1.2 主核编译器 (aarch64-linux-gnu-g++)
若采用在飞腾派上本地编译，直接使用原生 `g++` 即可；若在 x86_64 主机上进行交叉编译，需要安装交叉工具链：
```bash
sudo apt-get install -y g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
```

---

## 2. 核心运行时环境变量声明

Linux 侧的 `ppe_system` 依靠环境变量进行功能调度与降级防御，需在启动脚本中进行相应声明：

| 环境变量名称 | 候选值类型 | 示例配置 | 功能描述与降级防御 |
| :--- | :--- | :--- | :--- |
| **`DEEPSEEK_API_KEY`** | `mock` / 真实 Key | `export DEEPSEEK_API_KEY=sk-xxxx` | API Key。如果为 `mock`，系统将绕过实际网络请求，自动使用本地预置 advice；如果是真实 Key，则通过 Qt 网络模块发送真实的 JSON 大模型请求。 |
| **`DEEPSEEK_API_URL`** | URL 字符串 | `export DEEPSEEK_API_URL=https://api.deepseek.com/v1` | 大语言模型网关。可以重定向至本地私有部署的 LLM API 端口。 |
| **`DEEPSEEK_MOCK_OFFLINE`** | `1` / `0` | `export DEEPSEEK_MOCK_OFFLINE=1` | 离线注入开关。设为 `1` 时，强制 API 请求不发包并卡住 10 秒，以触发 UI 侧的“10秒超时失效降级”，模拟网络断连灾难。 |
| **`DISPLAY`** | X11 端口 | `export DISPLAY=:0` | 指定 Qt5 应用程序的显示屏。运行 Offscreen 测试模式时可忽略。 |

---

## 3. 自动化部署与远程一键构建 (temp_helper)

为了提高调试速度，本系统在 `temp_helper` 目录中集成了一套由 Python 编写的一键部署构建管道（Pipeline）：

```
    [ Windows 主机源码修改 ]
               │
               ▼  运行 temp_helper/sync_all.py
    [ SFTP 打包上传并解压至开发板 /home/user/ ]
               │
               ▼  运行 temp_helper/compile_slave_remote.py
    [ 远程触发 arm-none-eabi-gcc 编译从核固件 ]
               │
               ▼  运行 temp_helper/deploy_and_restart.py
    [ 固件拷贝至 /lib/firmware 目录并重启 remoteproc ]
               │
               ▼  运行 temp_helper/start_gui.py
    [ 远程拉起主核 ppe_system 程序运行并监控终端日志 ]
```

### 3.1 核心自动化脚本清单：
1.  **`sync_all.py`**：将 Windows 上的代码文件打包为 `project.tar.gz`，通过 HTTP 端口传输并解压至开发板上，完美绕过大型 SFTP 传输在网络代理下的阻塞丢包。
2.  **`compile_slave_remote.py`**：执行 SSH 远程控制指令，触发开发板上的 Baremetal 编译链。
3.  **`deploy_and_restart.py`**：实现对开发板 `/lib/firmware/` 目录的写入，向系统文件 `remoteproc0/state` 写入 `stop` 和 `start` 信号，重启从核 Core 1。
4.  **`start_gui.py`**：远程在指定大屏幕（`DISPLAY=:0`）上拉起主核 `ppe_system` 程序。
