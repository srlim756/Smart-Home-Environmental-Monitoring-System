#include "alarm.h"
#include "web.h"
#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static float thresholds[] = {
    [SENSOR_TEMP]  = TEMP_HIGH,
    [SENSOR_HUMI]  = 9999,
    [SENSOR_LIGHT] = 9999,
    [SENSOR_SMOKE] = SMOKE_HIGH,
};

static const char *sensor_names[] = {
    [SENSOR_TEMP]  = "TEMP",
    [SENSOR_HUMI]  = "HUMI",
    [SENSOR_LIGHT] = "LIGHT",
    [SENSOR_SMOKE] = "SMOKE",
};

void *processor_thread(void *arg) {
    processor_args_t *args = (processor_args_t *)arg;

    while (1) {
        sensor_data_t data;
        if (ringbuf_get(args->rb, &data) < 0)
            break;

        db_insert_sensor(args->db, &data);

        if (data.value > thresholds[data.type]) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "{\"type\":\"%s\",\"value\":%.1f,\"time\":%u}",
                     sensor_names[data.type], data.value, data.timestamp);

            printf("[ALARM] %s: %.1f (threshold: %.0f)\n",
                   sensor_names[data.type], data.value, thresholds[data.type]);

            mq_alarm_send(args->mq, msg);
            db_insert_alarm(args->db, data.type, data.value, msg);
        }
    }
    return NULL;
}

void *alarm_thread(void *arg) {
    mqd_t mq = *(mqd_t *)arg;
    char buf[512];

    while (1) {
        if (mq_alarm_recv(mq, buf, sizeof(buf)) > 0) {
            printf("[ALARM] Broadcast: %s\n", buf);
            web_broadcast_sse(buf);
        }
    }
    return NULL;
}
