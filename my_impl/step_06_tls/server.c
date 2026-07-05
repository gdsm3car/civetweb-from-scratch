#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* ======================= 回调机制核心 ======================= */

struct mg_connection {
    int client_fd;
    SSL *ssl;             /* NULL 表示 HTTP，非 NULL 表示 HTTPS */
};

typedef int (*request_handler)(struct mg_connection *conn, void *user_data);

struct handler_entry {
    char uri_pattern[64];
    request_handler handler;
    void *user_data;
};

#define MAX_HANDLERS 16

struct mg_context {
    struct handler_entry handlers[MAX_HANDLERS];
    int handler_count;
};

void mg_set_request_handler(struct mg_context *ctx,
                            const char *uri_pattern,
                            request_handler handler,
                            void *user_data) {
    if (ctx->handler_count >= MAX_HANDLERS) return;
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

/* 任务：包含 client_fd 和对应的 SSL 对象 */
struct task {
    int client_fd;
    SSL *ssl;
};

int que_size = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t pool[THREAD_POOL_SIZE];
struct task task_queue[QUEUE_SIZE];

/* ======================= OpenSSL 初始化 ======================= */

SSL_CTX *ssl_ctx = NULL;

void init_openssl() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

SSL_CTX *create_ssl_ctx() {
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX create failed\n");
        return NULL;
    }
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "load cert.pem failed\n");
        return NULL;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "load key.pem failed\n");
        return NULL;
    }
    return ctx;
}

/* ======================= 工具函数 ======================= */

const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".css") == 0) return "text/css";
    if (strcasecmp(ext, ".js") == 0) return "application/javascript";
    if (strcasecmp(ext, ".png") == 0) return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".gif") == 0) return "image/gif";
    if (strcasecmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcasecmp(ext, ".json") == 0) return "application/json";
    return "application/octet-stream";
}

char *prase_path(char *buffer) {
    int i = 0;
    while (buffer[i] != ' ' && buffer[i] != '\0' && buffer[i] != '\r') i++;
    if (buffer[i] == '\0' || buffer[i] == '\r') return NULL;
    int len = 0;
    i++;
    while (buffer[i + len] != ' ' && buffer[i + len] != '\0' && buffer[i + len] != '\r') len++;
    if (len == 0) return NULL;
    char *path = malloc(len + 1);
    memcpy(path, &buffer[i], len);
    path[len] = '\0';
    return path;
}

/* 统一读写：自动区分 HTTP 和 HTTPS */
int read_request(struct mg_connection *conn, char *buf, int size) {
    if (conn->ssl)
        return SSL_read(conn->ssl, buf, size);
    else
        return recv(conn->client_fd, buf, size, 0);
}

int write_response(struct mg_connection *conn, const char *buf, int len) {
    if (conn->ssl)
        return SSL_write(conn->ssl, buf, len);
    else
        return send(conn->client_fd, buf, len, 0);
}

int send_404(struct mg_connection *conn) {
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
    return write_response(conn, msg, strlen(msg));
}

/* ======================= 示例回调 ======================= */

int handle_api_hello(struct mg_connection *conn, void *user_data) {
    const char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 19\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"message\":\"hello\"}";
    write_response(conn, resp, strlen(resp));
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
    write_response(conn, resp, strlen(resp));
    return 1;
}

/* ======================= 静态文件处理（默认兜底） ======================= */

void serve_static_file(struct mg_connection *conn, const char *path) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "www%s", path);

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

        write_response(conn, header, header_len);
        if (conn->ssl)
            SSL_write(conn->ssl, content, fsize);
        else
            send(conn->client_fd, content, fsize, 0);
        free(content);
    } else if (strcmp(path, "/") == 0) {
        const char *msg =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello,world";
        write_response(conn, msg, strlen(msg));
    } else {
        send_404(conn);
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
        struct task t = task_queue[--que_size];
        pthread_mutex_unlock(&mutex);

        struct mg_connection conn;
        conn.client_fd = t.client_fd;
        conn.ssl = t.ssl;

        /* HTTPS 握手 */
        if (conn.ssl) {
            SSL_set_fd(conn.ssl, conn.client_fd);
            if (SSL_accept(conn.ssl) <= 0) {
                SSL_free(conn.ssl);
                close(conn.client_fd);
                continue;
            }
        }

        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        if (read_request(&conn, buffer, sizeof(buffer) - 1) == -1) {
            if (conn.ssl) SSL_free(conn.ssl);
            close(conn.client_fd);
            continue;
        }
        printf("%s\n", buffer);

        char *path = prase_path(buffer);
        if (path == NULL) {
            send_404(&conn);
            if (conn.ssl) SSL_free(conn.ssl);
            close(conn.client_fd);
            continue;
        }

        int handled = 0;
        for (int i = 0; i < ctx->handler_count; i++) {
            if (strcmp(path, ctx->handlers[i].uri_pattern) == 0) {
                ctx->handlers[i].handler(&conn, ctx->handlers[i].user_data);
                handled = 1;
                break;
            }
        }

        if (!handled) {
            serve_static_file(&conn, path);
        }

        free(path);
        if (conn.ssl) SSL_free(conn.ssl);
        close(conn.client_fd);
    }
    return NULL;
}

/* ======================= 监听服务 ======================= */

int create_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); return -1; }

    /* 允许端口复用 */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 10) == -1) {
        perror("listen"); close(fd); return -1;
    }
    return fd;
}

/* 传递 accept 参数的 struct */
struct accept_param {
    int server_fd;
    int is_https;
};

/* 接受连接并放入任务队列 */
void *accept_loop(void *arg) {
    struct accept_param *param = (struct accept_param *)arg;
    int server_fd = param->server_fd;
    int is_https = param->is_https;
    free(param);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }
        pthread_mutex_lock(&mutex);
        task_queue[que_size].client_fd = client_fd;
        task_queue[que_size].ssl = is_https ? SSL_new(ssl_ctx) : NULL;
        que_size++;
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond);
    }
    return NULL;
}

/* ======================= 入口 ======================= */

int main() {
    struct mg_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    mg_set_request_handler(&ctx, "/api/hello", handle_api_hello, NULL);
    mg_set_request_handler(&ctx, "/api/echo", handle_api_echo, NULL);

    /* 初始化 OpenSSL */
    init_openssl();
    ssl_ctx = create_ssl_ctx();
    if (!ssl_ctx) {
        printf("SSL init failed, still serving HTTP\n");
    }

    /* 创建线程池 */
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&pool[i], NULL, worker, &ctx);
    }

    /* 监听 HTTP 9090 端口 */
    int http_fd = create_listener(9090);
    if (http_fd == -1) { exit(EXIT_FAILURE); }
    printf("HTTP  server on http://localhost:9090\n");

    /* 监听 HTTPS 9443 端口（仅当 SSL 可用时） */
    int https_fd = -1;
    if (ssl_ctx) {
        https_fd = create_listener(9443);
        if (https_fd != -1) {
            printf("HTTPS server on https://localhost:9443\n");
        }
    }

    /* 同时 accept HTTP 和 HTTPS */
    pthread_t http_thread, https_thread;

    struct accept_param *http_param = malloc(sizeof(struct accept_param));
    http_param->server_fd = http_fd;
    http_param->is_https = 0;
    pthread_create(&http_thread, NULL, accept_loop, http_param);

    if (https_fd != -1) {
        struct accept_param *https_param = malloc(sizeof(struct accept_param));
        https_param->server_fd = https_fd;
        https_param->is_https = 1;
        pthread_create(&https_thread, NULL, accept_loop, https_param);
        pthread_join(https_thread, NULL);
    }

    pthread_join(http_thread, NULL);
    close(http_fd);
    if (https_fd != -1) close(https_fd);
    if (ssl_ctx) SSL_CTX_free(ssl_ctx);
    return 0;
}
