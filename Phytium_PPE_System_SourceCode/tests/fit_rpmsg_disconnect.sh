#!/bin/bash
# ==========================================
# FIT 测试: RPMsg 从核断连检测
# 测试用例编号: TC-09
# 风险项: R1 - 通信不可靠
# 管控措施: 心跳检测 5s/15s
# ==========================================

echo "=== FIT-TC-09: 从核断连检测测试 ==="
echo "[前置条件] 系统正常运行中，RPMsg 连接正常"
echo ""

echo "[步骤1] 确认当前 RPMsg 状态..."
ls -la /dev/rpmsg0 2>/dev/null && echo "✅ RPMsg 设备存在" || echo "⚠️ RPMsg 设备不存在"
echo ""

echo "[步骤2] 注入故障: 停止从核..."
echo stop > /sys/class/remoteproc/remoteproc0/state 2>/dev/null
echo "⏳ 从核已停止，等待 15 秒观察主核检测..."
echo ""

echo "[步骤3] 倒计时等待..."
for i in $(seq 15 -1 1); do
    echo -ne "   剩余 ${i}s\r"
    sleep 1
done
echo ""
echo ""

echo "[步骤4] 检查主核日志..."
echo "   预期: 出现 '🚨 [心跳超时] 从核通信中断' 字样"
echo "   预期: UI 中 RPMsg 徽章变为红色 '● RPMsg 断连'"
echo ""

echo "[步骤5] 恢复: 重新启动从核..."
echo openamp_core0.elf > /sys/class/remoteproc/remoteproc0/firmware 2>/dev/null
echo start > /sys/class/remoteproc/remoteproc0/state 2>/dev/null
echo "⏳ 等待 10 秒验证心跳恢复..."
sleep 10
echo "   预期: 出现 '💚 [心跳] 从核通信恢复' 字样"
echo "   预期: UI 中 RPMsg 徽章变为绿色 '● RPMsg 正常'"
echo ""

echo "=== 测试完成 ==="
echo "请记录实测结果并截图。"
