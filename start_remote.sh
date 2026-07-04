#!/bin/bash
# start_remote.sh — Host A 传感器远程启动（TCP 模式）
# 用法:
#   ./start_remote.sh <Hub_IP> 9090

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_DIR="$ROOT_DIR/logs"

if [ $# -lt 2 ]; then
    echo "用法: ./start_remote.sh <Hub_IP> <端口>"
    echo ""
    echo "示例: ./start_remote.sh 192.168.43.40 9090"
    echo ""
    echo "Hub IP 在 start.sh 输出中查看"
    exit 1
fi

HUB_HOST=$1
HUB_PORT=$2

mkdir -p "$LOG_DIR"

echo "============================================"
echo "   传感器采集端（TCP 远程模式）"
echo "============================================"
echo ""
echo "Hub 地址: ${HUB_HOST}:${HUB_PORT}"
echo ""

cd "$ROOT_DIR"

./temp_sensor     "$HUB_HOST" "$HUB_PORT" > "$LOG_DIR/temp.log"     2>&1 &
echo "[OK] temp_sensor     (PID $!)"

./humidity_sensor "$HUB_HOST" "$HUB_PORT" > "$LOG_DIR/humidity.log"  2>&1 &
echo "[OK] humidity_sensor (PID $!)"

./light_sensor    "$HUB_HOST" "$HUB_PORT" > "$LOG_DIR/light.log"     2>&1 &
echo "[OK] light_sensor    (PID $!)"

./smoke_sensor    "$HUB_HOST" "$HUB_PORT" > "$LOG_DIR/smoke.log"     2>&1 &
echo "[OK] smoke_sensor    (PID $!)"

echo ""
echo "============================================"
echo "4 个传感器已启动，数据发送到 ${HUB_HOST}:${HUB_PORT}"
echo "按 Ctrl+C 停止"
echo "============================================"

trap "pkill -P $$ 2>/dev/null; exit 0" INT TERM
wait
