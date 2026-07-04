#include "web.h"
#include "common.h"
#include "ringbuf.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define PORT 8080
#define BUFSIZE 8192
#define SSE_CLIENTS 16

static int sse_fds[SSE_CLIENTS];
static int sse_count = 0;
static pthread_mutex_t sse_mutex = PTHREAD_MUTEX_INITIALIZER;
static ringbuf_t *g_rb = NULL;
static sqlite3 *g_db = NULL;

static const char *mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0)  return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0)   return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0)  return "image/png";
    if (strcmp(ext, ".svg") == 0)  return "image/svg+xml";
    return "text/plain";
}

static void http_response(int fd, int status, const char *status_text,
                          const char *content_type, const char *body, size_t body_len) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    write(fd, header, n);
    write(fd, body, body_len);
}

static void http_ok(int fd, const char *content_type, const char *body) {
    http_response(fd, 200, "OK", content_type, body, strlen(body));
}

static void http_not_found(int fd) {
    http_response(fd, 404, "Not Found", "text/plain", "404 Not Found", 13);
}

static void serve_static_file(int fd, const char *path) {
    char fullpath[512];
    if (path[0] == '/') {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
            snprintf(fullpath, sizeof(fullpath), "webroot/index.html");
        else
            snprintf(fullpath, sizeof(fullpath), "webroot%s", path);
    } else {
        snprintf(fullpath, sizeof(fullpath), "webroot/%s", path);
    }

    int f = open(fullpath, O_RDONLY);
    if (f < 0) { http_not_found(fd); return; }

    char buf[BUFSIZE];
    int n = read(f, buf, sizeof(buf));
    close(f);

    if (n <= 0) { http_not_found(fd); return; }
    http_response(fd, 200, "OK", mime_type(fullpath), buf, (size_t)n);
}

static void handle_api_current(int fd) {
    char json[512];
    int pos = 0;
    uint32_t now = (uint32_t)time(NULL);
    uint32_t max_ts = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{");

    const char *names[] = {"temp","humi","light","smoke"};
    for (int t = 0; t < SENSOR_COUNT; t++) {
        sensor_data_t data;
        float val = 0;
        uint32_t ts = 0;
        if (db_query_recent(g_db, (sensor_type_t)t, 1, &data) > 0) {
            val = data.value;
            ts = data.timestamp;
        }
        if (ts > max_ts) max_ts = ts;
        if (pos > 1) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos,
                        "\"%s\":%.1f,\"%s_ts\":%u",
                        names[t], val, names[t], ts);
    }
    int stale = (now - max_ts) > 5;
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"stale\":%s}", stale ? "true" : "false");
    http_ok(fd, "application/json", json);
}

static void handle_api_history(int fd, const char *query) {
    int type = 0, limit = 20;
    if (query) {
        int t, l;
        if (sscanf(query, "type=%d&limit=%d", &t, &l) >= 1) type = t;
        if (l > 0 && l < 200) limit = l;
    }

    sensor_data_t results[200];
    int count = db_query_recent(g_db, (sensor_type_t)type, limit, results);

    char json[8192];
    int pos = snprintf(json, sizeof(json), "[");
    for (int i = count - 1; i >= 0; i--) {
        if (pos > 1 && i < count - 1)
            pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{\"v\":%.1f,\"t\":%u}", results[i].value, results[i].timestamp);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]");
    http_ok(fd, "application/json", json);
}

static void handle_sse(int fd) {
    char header[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    write(fd, header, strlen(header));

    pthread_mutex_lock(&sse_mutex);
    if (sse_count < SSE_CLIENTS) {
        sse_fds[sse_count++] = fd;
        printf("[WEB] SSE client connected (total: %d)\n", sse_count);
    }
    pthread_mutex_unlock(&sse_mutex);

    char buf[64];
    while (read(fd, buf, sizeof(buf)) > 0) {}

    pthread_mutex_lock(&sse_mutex);
    for (int i = 0; i < sse_count; i++) {
        if (sse_fds[i] == fd) {
            sse_fds[i] = sse_fds[--sse_count];
            break;
        }
    }
    printf("[WEB] SSE client disconnected (total: %d)\n", sse_count);
    pthread_mutex_unlock(&sse_mutex);
    close(fd);
}

static void *handle_request(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUFSIZE];
    int n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(client_fd); return NULL; }
    buf[n] = '\0';

    char method[16], path[256];
    if (sscanf(buf, "%15s %255s", method, path) < 2) {
        close(client_fd);
        return NULL;
    }

    if (strcmp(method, "GET") != 0) { close(client_fd); return NULL; }

    if (strcmp(path, "/api/events") == 0) {
        handle_sse(client_fd);
        return NULL;
    }

    if (strcmp(path, "/api/current") == 0) {
        handle_api_current(client_fd);
        close(client_fd);
        return NULL;
    }

    if (strncmp(path, "/api/history?", 13) == 0) {
        handle_api_history(client_fd, path + 13);
        close(client_fd);
        return NULL;
    }

    serve_static_file(client_fd, path);
    close(client_fd);
    return NULL;
}

void *web_thread(void *arg) {
    web_args_t *wa = (web_args_t *)arg;
    g_rb = wa->rb;

    signal(SIGPIPE, SIG_IGN);
    sqlite3_open("smarthome.db", &g_db);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("[WEB] socket"); return NULL; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[WEB] bind"); close(server_fd); return NULL;
    }
    if (listen(server_fd, 8) < 0) {
        perror("[WEB] listen"); close(server_fd); return NULL;
    }

    printf("[WEB] HTTP server on http://localhost:%d\n", PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("[WEB] accept"); break;
        }

        int *cfd = malloc(sizeof(int));
        *cfd = client_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_request, cfd);
        pthread_detach(tid);
    }

    close(server_fd);
    return NULL;
}

void web_broadcast_sse(const char *msg) {
    pthread_mutex_lock(&sse_mutex);
    for (int i = 0; i < sse_count; i++) {
        char buf[1024];
        int n = snprintf(buf, sizeof(buf), "data: %s\n\n", msg);
        write(sse_fds[i], buf, n);
    }
    pthread_mutex_unlock(&sse_mutex);
}
