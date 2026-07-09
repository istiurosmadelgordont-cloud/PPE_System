#!/bin/bash
# ==========================================
# FIT 测试: 主核进程挂起心跳丢失与从核接管自愈
# 测试用例编号: TC-12
# 风险项: R2 - 主控进程假死/挂起
# 管控措施: 异构双向心跳 + 物理看门狗
# ==========================================

echo "=== FIT-TC-12: 主核心跳丢失与从核自愈测试 ==="
echo "[前置条件] 系统正常运行中，物理从核（Core 1）与主核建立 RPMsg 正常心跳连接"
echo ""

# 1. 检查主进程是否运行
PID=$(pgrep ppe_system)
if [ -z "$PID" ]; then
    echo "⚠️ 错误: 检测到 ppe_system 主进程未运行！"
    echo "请在前台或后台通过 ./run_real_deepseek.sh 启动系统后重试。"
    exit 1
fi

echo "✅ 检测到主进程运行中 (PID: $PID)"
echo "--------------------------------------------------"
echo "提示："
echo "本次测试将向主进程发送 STOP 信号暂停其全部线程的运行（模拟死锁/假死）。"
echo "在此期间："
echo "1. 从核会在 2s 内（4次心跳丢失）检测到连接中断，并在串口打印失联日志。"
echo "2. 从核将接管报警并触发蜂鸣器长鸣。"
echo "3. 如果保持挂起状态超过 10s，硬件看门狗将触发整机重启自愈。"
echo "--------------------------------------------------"
echo ""

# 选择测试模式
echo "请输入测试模式序号："
echo "1) 挂起 1 秒后自动恢复（验证从核心跳丢失检测 + 自动恢复，不触发重启）"
echo "2) 永久挂起并等待重启（验证看门狗物理冷复位整机，完整自愈链路）"
read -p "您的选择 (1 或 2): " MODE

if [ "$MODE" = "1" ]; then
    echo ""
    echo "👉 正在准备注入故障：将在 3 秒后发送 STOP 信号（挂起 1 秒）..."
    sleep 3
    
    echo "⚡ [故障注入] 发送 STOP 信号挂起进程 $PID"
    echo "user" | sudo -S kill -STOP $PID
    
    echo "⏳ 进程已挂起 1 秒，请观察从核串口(COM9)的 heartbeat_miss 计数..."
    sleep 1
    
    echo "🔌 [故障撤销] 发送 CONT 信号恢复进程 $PID 运行"
    echo "user" | sudo -S kill -CONT $PID
    echo "✅ 进程已恢复运行！"
    sleep 2
    
    echo ""
    echo "=== 主核侧日志证据 ==="
    echo "--- /tmp/run_real_deepseek.log 最近日志 ---"
    tail -20 /tmp/run_real_deepseek.log | grep -E '心跳检测|ALERT|RPMsg|断开'
    echo "---"
    echo ""
    echo "=== 测试结果判定 ==="
    echo "1. 从核串口(COM9)是否打印了 heartbeat_miss=1/4 或 2/4？ (应出现)"
    echo "2. 主核日志是否出现了 '心跳中断' 警告？ (应出现)"
    echo "3. 恢复后系统是否继续正常运行(未重启)？ (应正常)"
    echo "符合上述现象，则 TC-12 模式1 测试判定为：【PASS - 检测到异常并自动恢复】"
    
elif [ "$MODE" = "2" ]; then
    echo ""
    echo "👉 正在准备注入故障：将在 3 秒后发送 STOP 信号..."
    sleep 3
    
    echo "⚡ [故障注入] 发送 STOP 信号挂起进程 $PID"
    echo "user" | sudo -S kill -STOP $PID
    
    echo "⏳ 进程已永久挂起。系统停止喂狗，硬件看门狗将在约 10 秒后复位整机！"
    echo "请保持串口连接，观察从核输出，等待系统硬重启复活。"
    
else
    echo "❌ 无效的选择，测试退出。"
    exit 1
fi

echo ""
echo "=== 测试完成 ==="
