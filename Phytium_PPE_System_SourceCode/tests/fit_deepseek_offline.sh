#!/bin/bash
# ==========================================
# FIT 测试: DeepSeek API 断网降级
# 测试用例编号: TC-13
# 风险项: R8 - 网络不可达
# 管控措施: 自动降级到预设安全建议
# ==========================================

echo "=== FIT-TC-13: DeepSeek API 断网降级测试 ==="
echo "[前置条件] 系统正常运行中，网络连通"
echo ""

echo "[步骤1] 验证正常 API 调用..."
echo "   触发一个违规告警，观察 DeepSeek 面板"
echo "   预期: 面板显示 '🤖 DeepSeek 正在分析中...' 然后变为 AI 建议"
echo "   按 Enter 继续..."
read

echo "[步骤2] 注入故障: 断开网络..."
echo "   方法1: 拔掉网线"
echo "   方法2: sudo ifconfig eth0 down"
echo "   按 Enter 继续 (确认已断网)..."
read

echo "[步骤3] 再次触发违规告警..."
echo "   预期: 面板显示 '🤖 DeepSeek 正在分析中...' 然后 10s 后降级为预设建议"
echo "   预期: 主核日志出现 '📡 [DeepSeek] 网络不可达，使用降级预设建议'"
echo "   预期: 系统其他功能不受影响"
echo ""

echo "[步骤4] 恢复网络..."
echo "   方法1: 插回网线"
echo "   方法2: sudo ifconfig eth0 up"
echo ""

echo "=== 测试完成 ==="
echo "请记录 API 调用和降级行为并截图。"
