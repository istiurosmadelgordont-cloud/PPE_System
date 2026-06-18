#!/bin/bash
# ==========================================
# FIT 测试: 主核崩溃后从核安全状态
# 测试用例编号: TC-10
# 风险项: R2 - 主控程序跑飞
# 管控措施: 从核看门狗，失效即安全 (Fail-Safe)
# ==========================================

echo "=== FIT-TC-10: 主核崩溃安全状态测试 ==="
echo "[前置条件] 系统正常运行中，蜂鸣器处于静默状态"
echo ""

echo "[步骤1] 确认主系统正在运行..."
PID=$(pidof ppe_system)
if [ -z "$PID" ]; then
    echo "⚠️ ppe_system 未运行，请先启动系统"
    exit 1
fi
echo "✅ ppe_system 运行中 (PID: $PID)"
echo ""

echo "[步骤2] 触发一个 AI 违规告警..."
echo "   请手动触发一个违规（如不戴安全帽），确认蜂鸣器已启动"
echo "   按 Enter 继续..."
read

echo "[步骤3] 注入故障: 强制杀死主核进程..."
kill -9 $PID
echo "✅ 主系统已被强制终止"
echo ""

echo "[步骤4] 等待 12 秒，观察从核行为..."
for i in $(seq 12 -1 1); do
    echo -ne "   剩余 ${i}s\r"
    sleep 1
done
echo ""
echo ""

echo "[步骤5] 验证从核安全状态..."
echo "   预期: 蜂鸣器已自动关闭（失效即安全）"
echo "   预期: 从核串口日志出现 '主核心跳超时！从核进入安全状态 (Fail-Safe)'"
echo ""

echo "=== 测试完成 ==="
echo "请记录实测结果（蜂鸣器是否关闭）并截图从核串口日志。"
