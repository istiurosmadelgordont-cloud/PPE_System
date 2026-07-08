#!/bin/bash
MODEL_DIR="/home/user/Phytium_PPE_System_SourceCode/ppe_system/model"
mkdir -p "$MODEL_DIR"
echo "[Self-Healing] Downloading model files from GitHub..."
wget -q --timeout=15 --tries=3 -O "$MODEL_DIR/model1_int8.param.tmp" "https://raw.githubusercontent.com/istiurosmadelgordont-cloud/PPE_System/main/ppe_system/model/model1_int8.param"
wget -q --timeout=15 --tries=3 -O "$MODEL_DIR/model1_int8.bin.tmp" "https://raw.githubusercontent.com/istiurosmadelgordont-cloud/PPE_System/main/ppe_system/model/model1_int8.bin"

if [ -s "$MODEL_DIR/model1_int8.param.tmp" ] && [ -s "$MODEL_DIR/model1_int8.bin.tmp" ]; then
    mv "$MODEL_DIR/model1_int8.param.tmp" "$MODEL_DIR/model1_int8.param"
    mv "$MODEL_DIR/model1_int8.bin.tmp" "$MODEL_DIR/model1_int8.bin"
    echo "[Self-Healing] Download successful and replaced models."
    exit 0
else
    rm -f "$MODEL_DIR/model1_int8.param.tmp" "$MODEL_DIR/model1_int8.bin.tmp"
    echo "[Self-Healing] Download failed. Trying to restore from local backup..."
    BACKUP_DIR="/home/user/Phytium_PPE_System_SourceCode/Phytium_PPE_System_SourceCode/ppe_system/model"
    if [ -f "$BACKUP_DIR/model1_int8.param" ] && [ -f "$BACKUP_DIR/model1_int8.bin" ]; then
        cp "$BACKUP_DIR/model1_int8.param" "$MODEL_DIR/"
        cp "$BACKUP_DIR/model1_int8.bin" "$MODEL_DIR/"
        echo "[Self-Healing] Restored successfully from local backup."
        exit 0
    else
        echo "[Self-Healing] Local backup not found."
        exit 1
    fi
fi
