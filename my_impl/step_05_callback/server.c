#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

/* ======================= 回调机制核心 ======================= */

/* 连接对象 — 传给回调函数，让回调能读写数据 */
struct mg_connection {
    int client_fd;
};

/* 回调函数类型：接收连接 + 用户数据，返回 1 表示已处理 */
typedef int (*request_handler)(struct mg_connection *conn, void *user_data);

/* 一条回调注册记录 */
struct handler_entry {
    char uri_pattern[64];
    request_handler handler;
    void *user_data;
};

#define MAX_HANDLERS 16

/* 服务器上下文 */
struct mg_context {
    struct handler_entry handlers[MAX_HANDLERS];
    int handler_count;
};

/* 注册回调函数 */
void mg_set_request_handler(struct mg_context *ctx,
                            const char *uri_pattern,
                            request_handler handler,
                            void *user_data) {
    if (ctx->handler_count >= MAX_HANDLERS) {
        printf("handler limit reached\n");
        return;
    }
    struct handler_entry *entry = &ctx->handlers[ctx->handler_count];
    strncpy(entry->uri_pattern, uri_pattern, sizeof(entry->uri_pattern) - 1);
    entry->uri_pattern[sizeof(entry->uri_pattern) - 1] = '\0';
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->handler_count++;
}

/* ======================= 线程池 ======================= */

#define THREAD_POOL_SIZE 4
#define QUEUE_SIZE 64
int que_size = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t pool[THREAD_POOL_SIZE];
int task_queue[QUEUE_SIZE];

/* ======================= 工具函数 ======================= */

const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";

    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".css") == 0)
        return "text/css";
    if (strcasecmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcasecmp(ext, ".png") == 0)
        return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcasecmp(ext, ".ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, ".json") == 0)
        return "application/json";

    return "application/octet-stream";
}

char *prase_path(char *buffer) {
    int i = 0;
    while (buffer[i] != ' ' && buffer[i] != '\0' && buffer[i] != '\r')
        i++;
    if (buffer[i] == '\0' || buffer[i] == '\r') return NULL;
    int len = 0;
    i++;
    while (buffer[i + len] != ' ' && buffer[i + len] != '\0' && buffer[i + len] != '\r')
        len++;
    if (len == 0) return NULL;
    char *path = malloc(len + 1);
    memcpy(path, &buffer[i], len);
    path[len] = '\0';
    return path;
}

int send_404(int client_fd) {
    const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
    char msg[1024];
    sprintf(msg,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(body), body);
    return send(client_fd, msg, strlen(msg), 0);
}

/* ======================= 示例回调函数 ======================= */

int handle_api_hello(struct mg_connection *conn, void *user_data) {
    const char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 19\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"message\":\"hello\"}";
    send(conn->client_fd, resp, strlen(resp), 0);
    return 1;
}

int handle_api_echo(struct mg_connection *conn, void *user_data) {
    const char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 4\r\n"
        "Connection: close\r\n"
        "\r\n"
        "echo";
    send(conn->client_fd, resp, strlen(resp), 0);
    return 1;
}

/* ======================= 静态文件处理（默认兜底） ======================= */

void serve_static_file(int client_fd, const char *path) {
    char *root = "www";
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%s", root, path);

    FILE *fp = fopen(filepath, "rb");
    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *content = malloc(fsize);
        fread(content, 1, fsize, fp);
        fclose(fp);

        const char *mime = get_mime_type(path);
        char header[512];
        int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            mime, fsize);

        send(client_fd, header, header_len, 0);
        send(client_fd, content, fsize, 0);
        free(content);
    } else if (strcmp(path, "/") == 0) {
        const char *msg =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello,world";
        send(client_fd, msg, strlen(msg), 0);
    } else {
        send_404(client_fd);
    }
}

/* ======================= 工作线程 ======================= */

void *worker(void *arg) {
    struct mg_context *ctx = (struct mg_context *)arg;

    while (1) {
        pthread_mutex_lock(&mutex);
        while (que_size == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        int client_fd = task_queue[--que_size];
        pthread_mutex_unlock(&mutex);

        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_fd, buffer, sizeof(buffer) - 1, 0) == -1) {
            printf("recv error\n");
            close(client_fd);
            continue;
        }
        printf("%s\n", buffer);

        char *path = prase_path(buffer);
        if (path == NULL) {
            send_404(client_fd);
            close(client_fd);
            continue;
        }

        /* ★ 核心修改：先查回调表，再走默认文件处理 */
        int handled = 0;
        for (int i = 0; i < ctx->handler_count; i++) {
            if (strcmp(path, ctx->handlers[i].uri_pattern) == 0) {
                struct mg_connection conn;
                conn.client_fd = client_fd;
                ctx->handlers[i].handler(&conn, ctx->handlers[i].user_data);
                handled = 1;
                break;
            }
        }

        if (!handled) {
            serve_static_file(client_fd, path);
        }

        close(client_fd);
        free(path);
    }
    return NULL;
}

/* ======================= 入口 ======================= */

int main() {
    struct mg_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* 注册回调（用户自定义的请求处理函数） */
    mg_set_request_handler(&ctx, "/api/hello", handle_api_hello, NULL);
    mg_set_request_handler(&ctx, "/api/echo", handle_api_echo, NULL);

    /* 创建线程池，把 ctx 传给 worker */
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&pool[i], NULL, worker, &ctx);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { printf("socket error\n"); exit(EXIT_FAILURE); }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("bind error\n"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) == -1) {
        printf("listen error\n"); exit(EXIT_FAILURE);
    }

    socklen_t addr_len = sizeof(client_addr);
    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            printf("accept error\n");
            continue;
        }
        pthread_mutex_lock(&mutex);
        task_queue[que_size++] = client_fd;
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond);
    }

    close(server_fd);
    return 0;
}
