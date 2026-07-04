#include "ipc.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ===== Unix Domain Socket ===== */

int ipc_server_create(const char *path) {
    unlink(path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 8) < 0) {
        perror("listen"); close(fd); return -1;
    }
    return fd;
}

int ipc_server_accept(int server_fd) {
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int cli = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (cli < 0) perror("accept");
    return cli;
}

void ipc_server_close(int server_fd, const char *path) {
    close(server_fd);
    unlink(path);
}

int ipc_client_connect(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return -1;
    }
    return fd;
}

/* ===== Generic socket read/write ===== */

int ipc_send(int fd, const void *data, size_t len) {
    size_t total = 0;
    while (total < len) {
        int n = write(fd, (const char *)data + total, len - total);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return 0;
}

int ipc_recv(int fd, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return 0;
}

void ipc_close(int fd) {
    close(fd);
}

/* ===== TCP Socket ===== */

int tcp_server_create(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 8) < 0) {
        perror("listen"); close(fd); return -1;
    }
    return fd;
}

int tcp_server_accept(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int cli = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (cli < 0) { perror("accept"); return -1; }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    printf("[TCP] Client connected from %s:%d\n", ip, ntohs(client_addr.sin_port));
    return cli;
}

int tcp_client_connect(const char *host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc)); return -1; }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { perror("socket"); freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect"); close(fd); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* ===== POSIX Message Queue ===== */

mqd_t mq_alarm_create(void) {
    mq_unlink(MQ_ALARM_NAME);
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = 256, .mq_curmsgs = 0 };
    mqd_t mq = mq_open(MQ_ALARM_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) perror("mq_open");
    return mq;
}

mqd_t mq_alarm_open(void) {
    mqd_t mq = mq_open(MQ_ALARM_NAME, O_RDWR);
    if (mq == (mqd_t)-1) perror("mq_open");
    return mq;
}

int mq_alarm_send(mqd_t mq, const char *msg) {
    if (mq_send(mq, msg, strlen(msg) + 1, 0) < 0) { perror("mq_send"); return -1; }
    return 0;
}

int mq_alarm_recv(mqd_t mq, char *buf, size_t len) {
    int n = (int)mq_receive(mq, buf, len, NULL);
    if (n < 0 && errno != EAGAIN) perror("mq_receive");
    return n;
}

void mq_alarm_close(mqd_t mq) {
    mq_close(mq);
}

/* ===== SysV Shared Memory ===== */

int shm_stop_create(void) {
    int shmid = shmget(SHM_KEY, sizeof(int), IPC_CREAT | IPC_EXCL | 0644);
    if (shmid < 0) {
        if (errno == EEXIST)
            shmid = shmget(SHM_KEY, sizeof(int), 0644);
        else { perror("shmget"); return -1; }
    }
    return shmid;
}

void *shm_stop_attach(int shmid) {
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *)-1) { perror("shmat"); return NULL; }
    *(int *)addr = 0;
    return addr;
}

void shm_stop_detach(void *addr) {
    shmdt(addr);
}

void shm_stop_set(void *addr, int val) {
    if (addr) *(int *)addr = val;
}

int shm_stop_check(void *addr) {
    return addr ? *(int *)addr : 1;
}
