#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/un.h>
#include <mqueue.h>
#include <sys/shm.h>
#include <time.h>

/*
 * IPC 路径与标识符
 * UDS: 本地传感器通信
 * TCP: 远程传感器通信
 * POSIX MQ: 异步告警通道
 * SysV shm: 跨进程停止标记
 */
#define SOCKET_PATH      "/tmp/smarthome_hub.sock"
#define TCP_DEFAULT_PORT 9090
#define MQ_ALARM_NAME    "/smarthome_alarm"
#define SHM_KEY          0x1234

/* 传感器基准值（随机游走算法中心点） */
#define TEMP_BASE    25.0f
#define HUMI_BASE    55.0f
#define LIGHT_BASE   400.0f
#define SMOKE_BASE   50.0f

/* 告警阈值 */
#define TEMP_HIGH    38.0f
#define SMOKE_HIGH   80.0f

/* 传感器类型枚举（作为数组下标使用，须从0开始连续） */
typedef enum {
    SENSOR_TEMP,    /* 温度 */
    SENSOR_HUMI,    /* 湿度 */
    SENSOR_LIGHT,   /* 光照 */
    SENSOR_SMOKE,   /* 烟雾 */
    SENSOR_COUNT    /* 传感器总数 4 */
} sensor_type_t;

/*
 * 传感器数据帧，16字节定长结构体
 * packed 禁止编译器填充，确保线缆传输格式固定
 * 传感器→集线器使用二进制格式，Web 层转为 JSON
 */
typedef struct {
    sensor_type_t type;       /* 传感器类型 */
    float         value;      /* 读数 */
    uint32_t      timestamp;  /* Unix 时间戳（秒） */
    uint8_t       reserved[4]; /* 保留，对齐到16字节 */
} __attribute__((packed)) sensor_data_t;

/* 获取毫秒级时间戳 */
static inline uint32_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#endif /* COMMON_H */
