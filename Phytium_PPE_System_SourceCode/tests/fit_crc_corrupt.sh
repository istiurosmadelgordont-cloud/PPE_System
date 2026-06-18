#!/bin/bash
# ==========================================
# FIT 测试: CRC8 数据完整性校验
# 测试用例编号: TC-11
# 风险项: R3 - 安全数据损坏
# 管控措施: CRC8 校验 + 丢弃
# ==========================================

echo "=== FIT-TC-11: CRC 数据完整性校验测试 ==="
echo "[前置条件] 系统正常运行中，RPMsg 连接正常"
echo ""

echo "[步骤1] 记录当前 CRC 错误计数 (观察 UI 面板)..."
echo "   当前 CRC 计数: ___ 次 (手动记录)"
echo ""

echo "[步骤2] 注入故障: 向 rpmsg0 发送随机数据..."
for i in $(seq 1 5); do
    dd if=/dev/urandom bs=262 count=1 > /dev/rpmsg0 2>/dev/null
    echo "   发送第 $i 包随机数据"
    sleep 0.5
done
echo ""

echo "[步骤3] 检查系统响应..."
echo "   预期: 主核日志出现 '⚠️ [CRC] 数据校验失败 #N，丢弃该包'"
echo "   预期: UI 中 CRC 计数增长到 5+"
echo "   预期: 系统功能不受影响，不崩溃"
echo ""

echo "[步骤4] 检查从核响应..."
echo "   预期: 从核串口出现 'CRC8 verify failed!'"
echo "   预期: 从核正常运行，不崩溃"
echo ""

echo "=== 测试完成 ==="
echo "请记录实测 CRC 计数变化并截图。"
