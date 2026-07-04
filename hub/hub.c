#include "common.h"
#include "ipc.h"
#include "ringbuf.h"
#include "db.h"
#include "alarm.h"
#include "web.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>

static volatile int g_running = 1;
static void *g_stop_flag = NULL;
static int g_shmid = -1;
static ringbuf_t g_rb;

static void handle_signal(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n[HUB] Shutting down...\n", 24);
    g_running = 0;
    if (g_stop_flag) shm_stop_set(g_stop_flag, 1);
}

void *receiver_thread(void *arg) {
    int *slot = (int *)arg;
    int fd = *slot;

    sensor_data_t data;
    while (g_running) {
        if (ipc_recv(fd, &data, sizeof(data)) < 0) {
            if (g_running)
                fprintf(stderr, "[HUB] Sensor fd=%d disconnected\n", fd);
            break;
        }
        ringbuf_put(&g_rb, &data);

        const char *names[] = {"TEMP","HUMI","LIGHT","SMOKE"};
        if (data.type < 4)
            printf("[HUB] <- %s: %.1f\n", names[data.type], data.value);
    }
    close(fd);
    *slot = -1;
    return NULL;
}

static int wait_for_sensors(int uds_fd, int tcp_fd,
                            int *client_fds, int max_clients) {
    int count = 0;
    printf("[HUB] Waiting for %d sensors (UDS+TCP)...\n", max_clients);

    while (count < max_clients && g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;
        if (uds_fd >= 0) { FD_SET(uds_fd, &readfds); maxfd = uds_fd; }
        if (tcp_fd >= 0) { FD_SET(tcp_fd, &readfds); if (tcp_fd > maxfd) maxfd = tcp_fd; }

        struct timeval tv = {1, 0};
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        if (uds_fd >= 0 && FD_ISSET(uds_fd, &readfds)) {
            int cli = ipc_server_accept(uds_fd);
            if (cli >= 0) {
                client_fds[count++] = cli;
                printf("[HUB] UDS sensor connected (%d/%d)\n", count, max_clients);
            }
        }

        if (tcp_fd >= 0 && FD_ISSET(tcp_fd, &readfds) && count < max_clients) {
            int cli = tcp_server_accept(tcp_fd);
            if (cli >= 0) {
                client_fds[count++] = cli;
                printf("[HUB] TCP sensor connected (%d/%d)\n", count, max_clients);
            }
        }
    }
    return count;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int uds_fd = ipc_server_create(SOCKET_PATH);
    if (uds_fd < 0) { fprintf(stderr, "[HUB] Failed to create UDS socket\n"); return 1; }
    printf("[HUB] UDS started on %s\n", SOCKET_PATH);

    int tcp_fd = tcp_server_create(TCP_DEFAULT_PORT);
    if (tcp_fd < 0) {
        fprintf(stderr, "[HUB] WARNING: TCP server not available\n");
    } else {
        printf("[HUB] TCP started on port %d\n", TCP_DEFAULT_PORT);
    }

    sqlite3 *db = NULL;
    if (db_init(&db) < 0) { fprintf(stderr, "[HUB] Failed to init DB\n"); return 1; }

    ringbuf_init(&g_rb);

    mqd_t mq = mq_alarm_create();
    if (mq == (mqd_t)-1) { fprintf(stderr, "[HUB] Failed to create mq\n"); return 1; }

    g_shmid = shm_stop_create();
    g_stop_flag = shm_stop_attach(g_shmid);

    web_args_t wargs = {&g_rb, mq};
    pthread_t web_tid;
    pthread_create(&web_tid, NULL, web_thread, &wargs);
    pthread_detach(web_tid);

    processor_args_t pargs = {&g_rb, db, mq};
    pthread_t proc_tid;
    pthread_create(&proc_tid, NULL, processor_thread, &pargs);
    pthread_detach(proc_tid);

    pthread_t alarm_tid;
    pthread_create(&alarm_tid, NULL, alarm_thread, &mq);
    pthread_detach(alarm_tid);

    int client_fds[4] = {-1, -1, -1, -1};
    int connected = wait_for_sensors(uds_fd, tcp_fd, client_fds, 4);
    printf("[HUB] All %d sensors connected\n", connected);

    if (!g_running) goto cleanup;

    for (int i = 0; i < 4; i++) {
        if (client_fds[i] < 0) continue;
        pthread_t tid;
        pthread_create(&tid, NULL, receiver_thread, &client_fds[i]);
        pthread_detach(tid);
    }

    while (g_running && !shm_stop_check(g_stop_flag)) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;
        if (uds_fd >= 0) { FD_SET(uds_fd, &readfds); maxfd = uds_fd; }
        if (tcp_fd >= 0) { FD_SET(tcp_fd, &readfds); if (tcp_fd > maxfd) maxfd = tcp_fd; }

        struct timeval tv = {1, 0};
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        int slot = -1;
        for (int i = 0; i < 4; i++) {
            if (client_fds[i] < 0) { slot = i; break; }
        }
        if (slot < 0) continue;

        int cli = -1;
        if (uds_fd >= 0 && FD_ISSET(uds_fd, &readfds))
            cli = ipc_server_accept(uds_fd);
        else if (tcp_fd >= 0 && FD_ISSET(tcp_fd, &readfds))
            cli = tcp_server_accept(tcp_fd);

        if (cli >= 0) {
            client_fds[slot] = cli;
            pthread_t tid;
            pthread_create(&tid, NULL, receiver_thread, &client_fds[slot]);
            pthread_detach(tid);
            printf("[HUB] Sensor reconnected on slot %d (fd=%d)\n", slot, cli);
        }
    }

cleanup:
    printf("[HUB] Cleaning up...\n");
    ringbuf_stop(&g_rb);
    sleep(1);
    ipc_server_close(uds_fd, SOCKET_PATH);
    if (tcp_fd >= 0) close(tcp_fd);
    mq_alarm_close(mq);
    mq_unlink(MQ_ALARM_NAME);
    db_close(db);
    shm_stop_detach(g_stop_flag);
    if (g_shmid >= 0) shmctl(g_shmid, IPC_RMID, NULL);
    printf("[HUB] Goodbye\n");
    return 0;
}
