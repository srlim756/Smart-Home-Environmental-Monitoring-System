# Smart Home Environmental Monitoring System

基于 Linux 多进程/多线程架构的智能家居环境监控系统，纯 C 语言实现。数据路径零动态内存分配，无第三方网络库依赖，支持 ARM 交叉编译。

## 系统架构

```
浏览器仪表盘 (http://IP:8080)
         ↑ HTTP + SSE
    ┌────┴──────────────┐
    │    Hub 中心进程    │
    │  ├ recv_thread ×4 │← Unix Domain Socket (或 TCP) ← 传感器进程 ×4
    │  ├ proc_thread    │→ SQLite (WAL 模式)
    │  ├ alarm_thread   │← POSIX 消息队列（异步告警）
    │  └ web_thread     │→ HTTP REST API + SSE 推送
    └───────────────────┘
```

- **Hub 中心进程**：4 个接收线程 + 处理线程 + 告警线程 + Web 线程
- **传感器进程**：4 个独立进程（温度/湿度/光照/烟雾），随机游走算法模拟
- **进程间通信**：四种 IPC 机制按场景选用（见下方）
- **数据持久化**：SQLite WAL 模式，读写互不阻塞
- **Web 仪表盘**：实时推送，无 JavaScript 框架依赖

## 技术亮点

### 1. 四种 IPC 按场景选用

| IPC 机制 | 使用场景 | 选型理由 |
|---|---|---|
| Unix Domain Socket | 传感器 → Hub（本地） | 同主机最低延迟、最高吞吐 |
| TCP Socket | 传感器 → Hub（远程） | 支持跨网络传感器部署 |
| POSIX 消息队列 | processor → alarm 线程 | 异步解耦；支持优先级；内核持久化 |
| SysV 共享内存 | stop.sh → Hub | 简单停止标记；外部脚本可直接访问 |

UDS 和 TCP 共用同一套 `ipc_send`/`ipc_recv` 接口——Unix "一切皆文件描述符" 的设计让两者使用相同的读写代码。

### 2. Ring Buffer 无锁数据管道

```
传感器 → UDS → recv_thread → [Ring Buffer] → proc_thread → SQLite
```

- 固定大小环形缓冲区（256 条目），**运行时零动态内存分配**
- `pthread_mutex` + `pthread_cond` 实现生产者-消费者同步
- `count = -1` 哨兵值实现优雅关闭——broadcast 唤醒所有阻塞线程

### 3. 自研 HTTP 服务器 + SSE 实时推送

- **零第三方依赖**——基于原生 TCP socket 手写 HTTP/1.1 服务器
- REST 接口：`/api/current`（最新值）、`/api/history`（历史查询）
- SSE (Server-Sent Events) 实时告警推送
- 每个请求创建独立线程处理

### 4. SQLite WAL 并发设计

- **WAL 模式**：写入不阻塞读取——Web 查询与传感器数据落库可同时进行
- 预编译语句提升批量写入性能，防止 SQL 注入
- 两张表：`sensor_log`（时序数据）、`alarm_log`（告警记录）

### 5. ARM 交叉编译

- `make arm` 目标，使用 `arm-linux-gnueabihf-gcc`
- QEMU 全系统仿真脚本（`scripts/run_arm.sh`），Cortex-A15 环境验证
- 传感器数据管道和 IPC 抽象层与硬件无关，同一份代码可编译 x86 和 ARM

### 6. 容错机制

- **自动重连**：传感器检测到连接断开后 1 秒重试
- **优雅关闭**：`SIGINT`/`SIGTERM` 信号处理 + SysV 共享内存停止标记 + Ring Buffer broadcast 唤醒
- **资源清理**：退出时自动删除 socket 文件、消息队列、共享内存

## 快速开始

```bash
# Ubuntu 依赖
sudo apt install gcc make libsqlite3-dev -y

# 编译运行
make
chmod +x start.sh stop.sh
./start.sh

# 仪表盘: http://localhost:8080
# 停止: ./stop.sh
```

### 远程传感器（TCP 模式）

```bash
# Hub 端
./start.sh hub

# 远程传感器端（指定 Hub IP）
./start_remote.sh <hub_ip> 9090
```

### ARM 交叉编译

```bash
make arm
# 输出在 build_arm/ 目录
# 部署到 QEMU 或 ARM 开发板
```

## 项目结构

```
├── common/          # IPC 封装层、数据类型定义、工具函数
│   ├── common.h     # 传感器数据结构（16字节 packed）、阈值、时间工具
│   ├── ipc.h        # IPC 接口定义
│   └── ipc.c        # IPC 实现：UDS、TCP、POSIX MQ、SysV shm
├── hub/             # 中心处理进程
│   ├── hub.c        # 主程序：线程管理、select 重连
│   ├── ringbuf.c    # 线程安全环形缓冲区（mutex + condvar）
│   ├── db.c         # SQLite 数据库操作（预编译语句、WAL 模式）
│   ├── alarm.c      # 阈值检查 + 告警广播
│   └── web.c        # HTTP 服务器 + SSE 推送（原生 socket）
├── sensors/         # 传感器进程模拟
│   ├── sensor_common.c  # 随机游走数据生成
│   ├── temp_sensor.c
│   ├── humidity_sensor.c
│   ├── light_sensor.c
│   └── smoke_sensor.c
├── webroot/         # 仪表盘前端（纯 HTML/CSS/JS）
├── scripts/         # QEMU 部署脚本
├── db/              # 数据库 schema 参考
├── Makefile         # x86 + ARM 交叉编译目标
├── start.sh         # 系统启动脚本
└── stop.sh          # 系统停止脚本
```

## 关键设计决策

| 决策 | 理由 |
|---|---|
| 传感器数据走二进制 struct，Web 层才用 JSON | 16 字节定长，传输解析效率高；浏览器端需要 JSON 的灵活性 |
| Ring Buffer 放在 Hub 进程内而非再走 IPC | 同一地址空间的生产者-消费者，mutex+condvar 最优解 |
| 4 个接收线程对应 4 个传感器 | 一对一映射，避免多路复用复杂度，4 个传感器足够 |
| SQLite WAL 而非 MySQL/PostgreSQL | 嵌入式场景——无需独立数据库服务；单文件部署；WAL 实现读写并发 |
| 原生 socket 而非 libmicrohttpd/nginx | 无第三方依赖，适合资源受限的嵌入式平台 |



