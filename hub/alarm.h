#ifndef ALARM_H
#define ALARM_H

#include "common.h"
#include "ringbuf.h"
#include "db.h"
#include <mqueue.h>
#include <sqlite3.h>

/* processor 线程参数 */
typedef struct {
    ringbuf_t *rb;  /* 环形缓冲区，获取传感器数据 */
    sqlite3   *db;  /* 数据库连接，持久化数据 */
    mqd_t      mq;  /* 消息队列，发送告警通知 */
} processor_args_t;

void *processor_thread(void *arg);  /* 数据处理线程：Ring Buffer → SQLite → 阈值检查 → MQ */
void *alarm_thread(void *arg);      /* 告警广播线程：MQ → SSE 推送到浏览器 */

#endif
