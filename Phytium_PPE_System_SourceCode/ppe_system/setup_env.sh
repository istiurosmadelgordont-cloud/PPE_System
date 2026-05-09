#!/bin/bash
echo "=== 启动：飞腾派硬件中断隔离 ==="
echo 0 > /proc/sys/kernel/numa_balancing 2>/dev/null || true
systemctl stop irqbalance 2>/dev/null
echo c > /proc/irq/93/smp_affinity
echo c > /proc/irq/94/smp_affinity
echo 1 > /proc/irq/87/smp_affinity
echo "=== 环境武装完毕 ==="
