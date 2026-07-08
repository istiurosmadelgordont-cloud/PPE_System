#!/bin/bash
# ==========================================
# FIT 测试: AI 模型文件缺失安全退出
# 测试用例编号: TC-12
# 风险项: R5 - 安全功能失效
# 管控措施: 加载失败安全退出
# ==========================================

echo "=== FIT-TC-12: 模型缺失安全退出测试 ==="
echo "[前置条件] 系统未运行"
echo ""

MODEL_DIR="/home/user/Phytium_PPE_System_SourceCode/ppe_system/model"
PARAM_FILE="$MODEL_DIR/model1_int8.param"
BACKUP_FILE="$MODEL_DIR/model1_int8.param.bak"

echo "[步骤1] 备份模型文件..."
if [ -f "$PARAM_FILE" ]; then
    cp "$PARAM_FILE" "$BACKUP_FILE"
    echo "✅ 模型已备份至 $BACKUP_FILE"
else
    echo "⚠️ 模型文件不存在: $PARAM_FILE"
    exit 1
fi
echo ""

echo "[步骤2] 注入故障: 删除模型文件..."
rm "$PARAM_FILE"
echo "✅ 模型文件已删除"
echo ""

echo "[步骤3] 启动系统，观察行为..."
echo "   请手动启动 ppe_system 并观察输出"
echo "   预期: 出现 '🚨 致命错误：模型加载失败' 或类似错误日志"
echo "   预期: 系统优雅退出，不发生段错误 (Segmentation Fault)"
echo "   按 Enter 继续 (完成观察后)..."
read

echo "[步骤4] 恢复模型文件..."
cp "$BACKUP_FILE" "$PARAM_FILE"
rm "$BACKUP_FILE"
echo "✅ 模型文件已恢复"
echo ""

echo "=== 测试完成 ==="
echo "请记录系统退出行为并截图日志。"
