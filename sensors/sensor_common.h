#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include "common.h"

/*
 * 随机游走算法生成模拟传感器数据
 * @param base       基准值，数据围绕此值波动
 * @param range      波动范围 ±range
 * @param counter    尖峰计数器，调用者维护
 * @param spike_ratio 尖峰强度，0 表示不触发尖峰
 * @return 生成的传感器值
 */
float sensor_random_walk(float base, float range, int *counter, float spike_ratio);

#endif
