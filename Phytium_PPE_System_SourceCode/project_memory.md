# PPE System — Project Memory & Hand-off Document

This document serves as the project memory for future Antigravity AI coding assistants to seamlessly resume work. It captures the project setup, network environment, critical bug fixes, DeepSeek implementation, and build instructions.

---

## 1. Environment & Network Setup
- **Windows Workspace**: `d:\飞腾派\CICC1004607+初赛+技术数据(代码类)\ppe4-28\Phytium_PPE_System_SourceCode`
- **Phytium Pi (Remote Board)**:
  - **Dynamic IP**: `172.20.10.4` (assigned via mobile hotspot DHCP).
  - **SSH Access**: Passwordless SSH keys configured from Windows host to remote board (`user@172.20.10.4`, password is `user`).
  - **Directory**: `/home/user/Phytium_PPE_System_SourceCode`
- **Proxy Workaround for Code Sync**:
  - Clash/Mihomo TUN mode on Windows blocks/resets large SFTP/SCP transfers (`Connection reset by peer`).
  - **Workaround pipeline**: 
    1. Package local Windows files using `create_archive.py` to `project.tar.gz` (excluding heavy build, SDK, models, and video files).
    2. Run local HTTP server: `python -m http.server 8000`.
    3. Run remote python script `download_and_extract.py` to download from local IP `172.20.10.3:8000` or `172.20.10.7:8000` and extract. This pipeline is 100% stable.

---

## 2. Git Configuration & Code Alignment
- **Branch**: `dev1`
- **Head Commit**: `c854441b80c4391e6b834927cb7040445d351be7` (pushed to GitHub repo `https://github.com/istiurosmadelgordont-cloud/PPE_System.git`).
- **Phytium Pi Git State**: Initialized, tracked to remote origin `dev1`, clean working tree matching Windows HEAD.

---

## 3. Critical Bug Fixes (Resolved Segfaults)

### SPSC Lockfree Queue Crashes
- **Problem**: Producer threads in `camera_node.cpp` and `inference_node.cpp` were calling `.pop()` when the SPSC queue was full to make space, which violated the Single Producer Single Consumer rule and corrupted head/tail indices, leading to immediate segment faults.
- **Fix**: Modified queue push routines to simply discard new frames/events when the queue is full instead of pop-push.

### Class Layout Mismatch Segfault
- **Problem**: Modifying `ui_main_window.hpp` to add new private variables changed the size of the `MainWindow` class. Due to incremental build cache on the remote board, only `ui_main_window.cpp` was recompiled. `main.cpp` and `rpmsg_node.cpp` still referenced the old class size, causing stack corruption and a segfault inside `addLogEntry`.
- **Fix**: Implemented a **clean build** strategy on the remote board by wiping `build/` files, running `cmake ..` and `make -j4`. This resolved the segfault.

---

## 4. DeepSeek AI Advisor Integration
- **Asynchronous Worker**: Implemented `deepseek_worker.hpp` and `deepseek_worker.cpp` inheriting from `QObject`. Handles network requests on background thread via Qt Network.
- **API Call & Key Configuration**: Configured through environment variables:
  - `DEEPSEEK_API_KEY`: If set to `mock` or empty, it runs in Mock mode. If set to a real DeepSeek API key, it attempts real API chat completions.
  - `DEEPSEEK_API_URL`: Configurable custom API gateway endpoint.
- **Fail-Safe & Timeout**: Uses a `QTimer` with a 10-second limit. If the network drops or requests timeout, it prints standard log:
  `📡 [DeepSeek] 网络不可达，使用降级预设建议`
  and falls back to pre-programmed HTML-formatted advice cards inside the UI.
- **FIT Testing (TC-13)**: Can be verified by running the system in mock mode:
  - **Online**: `export DEEPSEEK_API_KEY=mock` (will mock responses after 2s).
  - **Offline**: `export DEEPSEEK_API_KEY=mock` and `export DEEPSEEK_MOCK_OFFLINE=1` (will wait 10s and fall back with log output).

---

## 5. Build and Execution Instructions (Remote Board)

### Clean Compilation
```bash
cd /home/user/Phytium_PPE_System_SourceCode/ppe_system
mkdir -p build && cd build
rm -rf *
cmake ..
make -j4
```

### Running with Display (GUI Mode)
```bash
export DISPLAY=:0
sudo -E ./ppe_system
```

### Headless Verification (Offscreen Mode)
For command line testing or running FIT test scripts over SSH:
```bash
./ppe_system -platform offscreen
```
