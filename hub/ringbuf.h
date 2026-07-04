#ifndef RINGBUF_H
#define RINGBUF_H

#include "common.h"
#include <pthread.h>

#define RINGBUF_SIZE 256  /* 缓冲区容量（2的幂，便于取模） */

/* 线程安全环形缓冲区：生产者-消费者模型 */
typedef struct {
    sensor_data_t buffer[RINGBUF_SIZE]; /* 数据存储区 */
    int head;         /* 写入位置（生产者） */
    int tail;         /* 读取位置（消费者） */
    int count;        /* 当前元素数，-1 表示停止状态 */
    pthread_mutex_t mutex;      /* 互斥锁，保护临界区 */
    pthread_cond_t not_full;    /* 缓冲区不满条件变量 */
    pthread_cond_t not_empty;   /* 缓冲区不空条件变量 */
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb);               /* 初始化缓冲区 */
void ringbuf_stop(ringbuf_t *rb);               /* 停止缓冲区，唤醒所有等待线程 */
int  ringbuf_put(ringbuf_t *rb, const sensor_data_t *data);  /* 生产者：放入数据（阻塞） */
int  ringbuf_get(ringbuf_t *rb, sensor_data_t *data);        /* 消费者：取出数据（阻塞） */
int  ringbuf_try_get(ringbuf_t *rb, sensor_data_t *data);    /* 非阻塞尝试取出数据 */
int  ringbuf_peek_latest(ringbuf_t *rb, sensor_type_t type, sensor_data_t *out); /* 查看指定类型的最新数据 */

#endif
