#!/bin/bash

echo "=== 停止系统 ==="

# 通过共享内存通知 Hub 停止
HUB_PID=$(pgrep -x smarthome_hub 2>/dev/null || true)
if [ -n "$HUB_PID" ]; then
    kill -TERM "$HUB_PID" 2>/dev/null || true
fi

# 停止传感器进程
for proc in temp_sensor humidity_sensor light_sensor smoke_sensor; do
    pkill -x "$proc" 2>/dev/null || true
done

# 停止 web 子线程创建的连接处理
sleep 1

# 清理 tmux
tmux kill-session -t smarthome 2>/dev/null || true

# 清理 socket 文件
rm -f /tmp/smarthome_hub.sock

echo "=== 系统已停止 ==="
