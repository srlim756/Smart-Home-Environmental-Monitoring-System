#include "ringbuf.h"
#include <stdio.h>
#include <string.h>

void ringbuf_init(ringbuf_t *rb) {
    memset(rb, 0, sizeof(*rb));
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_full, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
}

void ringbuf_stop(ringbuf_t *rb) {
    pthread_mutex_lock(&rb->mutex);
    rb->count = -1;
    pthread_cond_broadcast(&rb->not_empty);
    pthread_cond_broadcast(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
}

int ringbuf_put(ringbuf_t *rb, const sensor_data_t *data) {
    pthread_mutex_lock(&rb->mutex);

    while (rb->count >= RINGBUF_SIZE && rb->count >= 0)
        pthread_cond_wait(&rb->not_full, &rb->mutex);

    if (rb->count < 0) { pthread_mutex_unlock(&rb->mutex); return -1; }

    rb->buffer[rb->head] = *data;
    rb->head = (rb->head + 1) % RINGBUF_SIZE;
    rb->count++;

    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

int ringbuf_get(ringbuf_t *rb, sensor_data_t *data) {
    pthread_mutex_lock(&rb->mutex);

    while (rb->count == 0)
        pthread_cond_wait(&rb->not_empty, &rb->mutex);

    if (rb->count < 0) { pthread_mutex_unlock(&rb->mutex); return -1; }

    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RINGBUF_SIZE;
    rb->count--;

    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

int ringbuf_try_get(ringbuf_t *rb, sensor_data_t *data) {
    pthread_mutex_lock(&rb->mutex);
    if (rb->count <= 0) {
        pthread_mutex_unlock(&rb->mutex);
        return (rb->count < 0) ? -2 : -1;
    }

    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RINGBUF_SIZE;
    rb->count--;

    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

int ringbuf_peek_latest(ringbuf_t *rb, sensor_type_t type, sensor_data_t *out) {
    pthread_mutex_lock(&rb->mutex);

    int idx = (rb->tail + rb->count - 1) % RINGBUF_SIZE;
    for (int i = 0; i < rb->count; i++) {
        int cur = (idx - i + RINGBUF_SIZE) % RINGBUF_SIZE;
        if (rb->buffer[cur].type == type) {
            *out = rb->buffer[cur];
            pthread_mutex_unlock(&rb->mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&rb->mutex);
    return -1;
}
