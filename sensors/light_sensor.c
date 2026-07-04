#include "sensor_common.h"
#include "ipc.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    srand((unsigned)(time(NULL) ^ getpid()));

    int use_tcp = (argc == 3);
    const char *host = use_tcp ? argv[1] : NULL;
    int port = use_tcp ? atoi(argv[2]) : 0;

    int fd;
    if (use_tcp) {
        fd = tcp_client_connect(host, port);
    } else {
        fd = ipc_client_connect(SOCKET_PATH);
    }
    if (fd < 0) {
        fprintf(stderr, "[LIGHT] Failed to connect to hub\n");
        return 1;
    }
    printf("[LIGHT] Connected via %s\n", use_tcp ? "TCP" : "UDS");

    int counter = 0;
    while (1) {
        sensor_data_t data;
        data.type = SENSOR_LIGHT;
        data.value = sensor_random_walk(LIGHT_BASE, 100.0f, &counter, 0.0f);
        data.timestamp = (uint32_t)(time(NULL));

        if (ipc_send(fd, &data, sizeof(data)) < 0) {
            fprintf(stderr, "[LIGHT] Connection lost, reconnecting...\n");
            close(fd);
            sleep(1);
            fd = use_tcp ? tcp_client_connect(host, port)
                         : ipc_client_connect(SOCKET_PATH);
            if (fd < 0) continue;
        }

        printf("[LIGHT] %.1f lux\n", data.value);
        sleep(1);
    }

    ipc_close(fd);
    return 0;
}
