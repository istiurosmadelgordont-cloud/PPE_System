#!/bin/bash
# ==========================================
# PPE System Real DeepSeek API Startup Script
# ==========================================

# 1. 填入您的真实 DeepSeek API Key (将下面的 PLACEHOLDER_KEY 替换为您的真实 Key)
export DEEPSEEK_API_KEY="sk-PLACEHOLDER_KEY"

# 2. 填入 API 网关地址与模型名称 (默认直连官方，可根据文档切换为 deepseek-v4-flash 或 deepseek-v4-pro)
export DEEPSEEK_API_URL="https://api.deepseek.com/chat/completions"
export DEEPSEEK_MODEL="deepseek-v4-flash"

# 3. 配置 GUI 显示权限与 X11 访问
export DISPLAY=:0
xhost + 2>/dev/null

# 4. 确保加载从核固件并启动从核 (Core 1)
echo "⚡ 正在加载从核固件并启动从核 (Core 1)..."
echo "user" | sudo -S modprobe rpmsg_char 2>/dev/null
echo "user" | sudo -S sh -c 'echo stop > /sys/class/remoteproc/remoteproc0/state' 2>/dev/null
echo "user" | sudo -S sh -c 'echo openamp_core0.elf > /sys/class/remoteproc/remoteproc0/firmware' 2>/dev/null
echo "user" | sudo -S sh -c 'echo start > /sys/class/remoteproc/remoteproc0/state' 2>/dev/null
sleep 1

# 5. 授予跨核硬件通信节点读写权限
sudo chmod 666 /dev/rpmsg_ctrl0 2>/dev/null
sudo chmod 666 /dev/rpmsg0 2>/dev/null

# 6. 以最高权限启动 PPE 监控系统并保留环境变量
echo "⚡ 正在启动 PPE 智能监控系统 (真实 DeepSeek API 模式)..."
cd "$(dirname "$0")/build"
echo "user" | sudo -S -E ./ppe_system
