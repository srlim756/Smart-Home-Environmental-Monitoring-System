#include "sensor_common.h"
#include <stdlib.h>

float sensor_random_walk(float base, float range, int *counter, float spike_ratio) {
    (*counter)++;
    float val = base + ((float)(rand() % 2000 - 1000) / 1000.0f) * range;

    if (*counter >= 20 + rand() % 10) {
        *counter = 0;
        val += base * spike_ratio;
    }

    if (val < 0) val = 0;
    return val;
}
