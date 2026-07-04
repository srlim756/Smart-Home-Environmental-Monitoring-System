#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mqueue.h>
#include <sys/shm.h>
#include <stddef.h>

/*
 * ===== Unix Domain Socket 接口 =====
 * 本地传感器进程 ↔ 集线器进程
 */
int  ipc_server_create(const char *path);   /* 创建 UDS 服务器，path 为 socket 文件路径 */
int  ipc_server_accept(int server_fd);      /* 接受客户端连接 */
void ipc_server_close(int server_fd, const char *path); /* 关闭服务器并删除 socket 文件 */
int  ipc_client_connect(const char *path);  /* 连接到 UDS 服务器 */

/*
 * ===== TCP Socket 接口 =====
 * 跨网络传感器进程 ↔ 集线器进程
 */
int  tcp_server_create(int port);           /* 创建 TCP 服务器，监听指定端口 */
int  tcp_server_accept(int server_fd);      /* 接受 TCP 客户端连接 */
int  tcp_client_connect(const char *host, int port); /* 连接到 TCP 服务器 */

/*
 * ===== 通用 Socket 读写接口（UDS 和 TCP 共用）=====
 * Unix "一切皆文件描述符" 设计，UDS 和 TCP 使用相同的 read/write 语义
 */
int  ipc_send(int fd, const void *data, size_t len); /* 循环写入，确保完整发送 */
int  ipc_recv(int fd, void *buf, size_t len);        /* 循环读取，确保完整接收 */
void ipc_close(int fd);                               /* 关闭文件描述符 */

/*
 * ===== POSIX 消息队列接口（告警通道）=====
 * processor 线程 → alarm 线程，异步解耦
 */
mqd_t mq_alarm_create(void);                /* 创建告警消息队列 */
mqd_t mq_alarm_open(void);                  /* 打开已存在的告警消息队列 */
int   mq_alarm_send(mqd_t mq, const char *msg);   /* 发送告警消息（JSON 格式） */
int   mq_alarm_recv(mqd_t mq, char *buf, size_t len); /* 接收告警消息 */
void  mq_alarm_close(mqd_t mq);             /* 关闭消息队列 */

/*
 * ===== SysV 共享内存接口（停止标记）=====
 * 外部进程（stop.sh）→ 集线器，通知优雅关闭
 */
int   shm_stop_create(void);                /* 创建共享内存段 */
void *shm_stop_attach(int shmid);           /* 附加到进程地址空间 */
void  shm_stop_detach(void *addr);          /* 分离共享内存 */
void  shm_stop_set(void *addr, int val);    /* 设置停止标记 */
int   shm_stop_check(void *addr);           /* 检查停止标记，0=运行，非0=停止 */

#endif /* IPC_H */
