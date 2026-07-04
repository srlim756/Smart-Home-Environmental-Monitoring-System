#!/bin/bash
# start.sh — 启动智能家居系统
# 用法:
#   ./start.sh             本地模式（中枢 + 4 个本地传感器）
#   ./start.sh hub         仅中枢（等待远程传感器连接）

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_DIR="$ROOT_DIR/logs"
MODE=${1:-local}
HUB_IP=$(hostname -I | awk '{print $1}')

mkdir -p "$LOG_DIR"

echo "============================================"
if [ "$MODE" = "hub" ]; then
    echo "   智能家居环境监控系统 [仅中枢模式]"
else
    echo "   智能家居环境监控系统 [本地模式]"
fi
echo "============================================"
echo ""

# 1. 编译
echo "[1/2] 编译..."
make -C "$ROOT_DIR" -s 2>&1 | tail -1
echo ""

# 2. 清理旧残骸
pkill -x smarthome_hub 2>/dev/null || true
pkill -x temp_sensor 2>/dev/null || true
pkill -x humidity_sensor 2>/dev/null || true
pkill -x light_sensor 2>/dev/null || true
pkill -x smoke_sensor 2>/dev/null || true
sleep 1
rm -f /tmp/smarthome_hub.sock

# 3. 启动中枢
echo "[2/2] 启动中枢..."
cd "$ROOT_DIR"
./smarthome_hub > "$LOG_DIR/hub.log" 2>&1 &
HUB_PID=$!
sleep 2

# 确认中枢启动成功
if ! kill -0 "$HUB_PID" 2>/dev/null; then
    echo "[ERROR] 中枢启动失败，检查日志: $LOG_DIR/hub.log"
    exit 1
fi

# 4. 本地模式：启动 4 个传感器
if [ "$MODE" = "hub" ]; then
    echo "  等待远程传感器连接..."
else
    echo "  [本地] 启动 4 个传感器..."
    ./temp_sensor     > "$LOG_DIR/temp.log"     2>&1 &
    ./humidity_sensor > "$LOG_DIR/humidity.log"  2>&1 &
    ./light_sensor    > "$LOG_DIR/light.log"     2>&1 &
    ./smoke_sensor    > "$LOG_DIR/smoke.log"     2>&1 &
fi

echo ""
echo "============================================"
echo ""
echo "  系统已启动！"
echo ""
echo "  Hub  PID: $HUB_PID"
echo "  本机 IP:  $HUB_IP"
echo "  日志目录: $LOG_DIR/"
echo ""
echo "  ============================================"
echo "  >> Web 仪表盘: http://localhost:8080     <<"
echo "  ============================================"
echo ""

if [ "$MODE" = "hub" ]; then
    echo "  仪表盘当前显示 ""传感器离线""（无传感器连接）"
    echo ""
    echo "  远程传感器运行:"
    echo "    ./start_remote.sh $HUB_IP 9090"
else
    echo "  远程传感器运行:"
    echo "    ./start_remote.sh $HUB_IP 9090"
fi

echo ""
echo "  停止系统: ./stop.sh"
echo "  查看日志: tail -f $LOG_DIR/hub.log"
echo ""
echo "============================================"
