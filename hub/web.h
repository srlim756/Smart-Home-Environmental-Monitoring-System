#ifndef WEB_H
#define WEB_H

#include "ringbuf.h"
#include <mqueue.h>

/* Web 线程参数 */
typedef struct {
    ringbuf_t *rb;  /* 环形缓冲区指针 */
    mqd_t      mq;  /* 告警消息队列 */
} web_args_t;

void *web_thread(void *arg);         /* HTTP 服务器主线程 */
void  web_broadcast_sse(const char *msg); /* 广播 SSE 消息给所有客户端 */

#endif
