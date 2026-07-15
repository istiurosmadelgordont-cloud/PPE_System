#!/bin/bash
# 自动修复脚本：使用相对路径和国内加速代理拉取模型

# 1. 动态获取相对路径，避免写死导致不同开发板上路径不一致
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MODEL_DIR="$SCRIPT_DIR/../model"
mkdir -p "$MODEL_DIR"

echo "[Self-Healing] 开始自动修复，准备拉取模型文件..."

# 2. 使用 ghproxy 代理加速下载（解决由于国内网络原因导致拉取 GitHub 失败）
BASE_URL="https://mirror.ghproxy.com/https://raw.githubusercontent.com/istiurosmadelgordont-cloud/PPE_System/main/ppe_system/model"

echo "[Self-Healing] 正在拉取 model1_int8.param..."
wget -q --timeout=15 --tries=3 -O "$MODEL_DIR/model1_int8.param.tmp" "$BASE_URL/model1_int8.param"

echo "[Self-Healing] 正在拉取 model1_int8.bin..."
wget -q --timeout=15 --tries=3 -O "$MODEL_DIR/model1_int8.bin.tmp" "$BASE_URL/model1_int8.bin"

if [ -s "$MODEL_DIR/model1_int8.param.tmp" ] && [ -s "$MODEL_DIR/model1_int8.bin.tmp" ]; then
    mv "$MODEL_DIR/model1_int8.param.tmp" "$MODEL_DIR/model1_int8.param"
    mv "$MODEL_DIR/model1_int8.bin.tmp" "$MODEL_DIR/model1_int8.bin"
    echo "[Self-Healing] 🎉 下载成功！模型已自动修复并替换就绪。"
    exit 0
else
    rm -f "$MODEL_DIR/model1_int8.param.tmp" "$MODEL_DIR/model1_int8.bin.tmp"
    echo "🚨 [AI 引擎] 致命错误：模型自愈失败，请检查网络连接及远程 GitHub 仓库！"
    exit 1
fi

