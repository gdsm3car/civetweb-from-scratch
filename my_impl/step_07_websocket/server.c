#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

/* ======================= 回调机制核心 ======================= */

struct mg_connection {
    int client_fd;
    SSL *ssl;
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

struct task {
    int client_fd;
    SSL *ssl;
};

int que_size = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t pool[THREAD_POOL_SIZE];
struct task task_queue[QUEUE_SIZE];

/* ======================= OpenSSL ======================= */

SSL_CTX *ssl_ctx = NULL;

void init_openssl() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

SSL_CTX *create_ssl_ctx() {
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return NULL;
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) return NULL;
    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) return NULL;
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
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return "image/jpeg";
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

int read_request(struct mg_connection *conn, char *buf, int size) {
    if (conn->ssl) return SSL_read(conn->ssl, buf, size);
    else return recv(conn->client_fd, buf, size, 0);
}

int write_response(struct mg_connection *conn, const char *buf, int len) {
    if (conn->ssl) return SSL_write(conn->ssl, buf, len);
    else return send(conn->client_fd, buf, len, 0);
}

/* 从 HTTP 请求中查找指定头部的值 */
char *get_header_value(const char *buffer, const char *header_name) {
    const char *p = strstr(buffer, "\r\n");
    if (!p) return NULL;
    p += 2;

    while (*p) {
        if (strncasecmp(p, header_name, strlen(header_name)) == 0) {
            p += strlen(header_name);
            while (*p == ' ' || *p == ':') p++;
            const char *end = strstr(p, "\r\n");
            if (!end) return NULL;
            size_t len = end - p;
            char *value = malloc(len + 1);
            memcpy(value, p, len);
            value[len] = '\0';
            return value;
        }
        p = strstr(p, "\r\n");
        if (!p) return NULL;
        p += 2;
        if (p[0] == '\r' && p[1] == '\n') break;
    }
    return NULL;
}

/* Base64 编码 */
void base64_encode(const unsigned char *input, int len, char *output) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j;
    for (i = 0, j = 0; i < len; i += 3) {
        int b = (input[i] << 16) | (i + 1 < len ? input[i + 1] << 8 : 0) | (i + 2 < len ? input[i + 2] : 0);
        output[j++] = table[(b >> 18) & 0x3F];
        output[j++] = table[(b >> 12) & 0x3F];
        output[j++] = (i + 1 < len) ? table[(b >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < len) ? table[b & 0x3F] : '=';
    }
    output[j] = '\0';
}

/* ======================= WebSocket 帧操作 ======================= */

/* 向客户端发送 WebSocket 帧（服务器 → 客户端，不掩码） */
void send_websocket_frame(struct mg_connection *conn, int opcode,
                          const char *data, size_t len) {
    unsigned char header[10];
    int hlen;

    header[0] = 0x80 | (opcode & 0x0F);  /* FIN + opcode */

    if (len < 126) {
        header[1] = (unsigned char)len;
        hlen = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (unsigned char)(len >> 8);
        header[3] = (unsigned char)(len & 0xFF);
        hlen = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++)
            header[2 + i] = (unsigned char)((len >> (56 - i * 8)) & 0xFF);
        hlen = 10;
    }

    write_response(conn, (const char *)header, hlen);
    write_response(conn, data, len);
}

/* 发送文本帧（opcode = 0x1） */
void send_websocket_text(struct mg_connection *conn, const char *text) {
    send_websocket_frame(conn, 0x1, text, strlen(text));
}

/* 发送关闭帧（opcode = 0x8） */
void send_websocket_close(struct mg_connection *conn) {
    send_websocket_frame(conn, 0x8, NULL, 0);
}

/* 读取并解析一个 WebSocket 帧，返回 payload（需要 free） */
char *read_websocket_frame(struct mg_connection *conn, size_t *out_len) {
    unsigned char header[2];
    if (read_request(conn, (char *)header, 2) <= 0) {
        *out_len = 0;
        return NULL;
    }

    int opcode = header[0] & 0x0F;
    int masked = (header[1] & 0x80) ? 1 : 0;
    size_t len = header[1] & 0x7F;

    /* 处理扩展长度 */
    if (len == 126) {
        unsigned char ext[2];
        if (read_request(conn, (char *)ext, 2) <= 0) { *out_len = 0; return NULL; }
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (read_request(conn, (char *)ext, 8) <= 0) { *out_len = 0; return NULL; }
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }

    /* 读取掩码 */
    unsigned char mask[4] = {0};
    if (masked) {
        if (read_request(conn, (char *)mask, 4) <= 0) { *out_len = 0; return NULL; }
    }

    /* 读取 payload */
    char *payload = malloc(len + 1);
    if (len > 0 && read_request(conn, payload, len) <= 0) {
        free(payload);
        *out_len = 0;
        return NULL;
    }
    payload[len] = '\0';

    /* 解码掩码 */
    if (masked) {
        for (size_t i = 0; i < len; i++)
            payload[i] ^= mask[i % 4];
    }

    /* 关闭帧 */
    if (opcode == 0x8) {
        free(payload);
        *out_len = 0;
        return NULL;
    }

    *out_len = len;
    return payload;
}

/* ======================= WebSocket 握手 ======================= */

int handle_websocket_upgrade(struct mg_connection *conn, const char *buffer) {
    char *key = get_header_value(buffer, "Sec-WebSocket-Key");
    if (!key) return -1;

    /* 计算 Sec-WebSocket-Accept: SHA1(key + magic) → Base64 */
    const char *magic = "258EAFA5-E914-47DA-95CA-5AB9DC11B85B";
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);
    free(key);

    unsigned char sha1_out[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)combined, strlen(combined), sha1_out);

    char accept_b64[64];
    base64_encode(sha1_out, SHA_DIGEST_LENGTH, accept_b64);

    /* 返回 101 Switching Protocols */
    char resp[512];
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_b64);

    write_response(conn, resp, len);
    return 0;
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

/* ======================= WebSocket Echo 处理 ======================= */

void handle_websocket_echo(struct mg_connection *conn) {
    /* 进入 WebSocket 循环 */
    while (1) {
        size_t len;
        char *data = read_websocket_frame(conn, &len);
        if (data == NULL) break;  /* 连接关闭或出错 */

        printf("WebSocket received: %s\n", data);
        send_websocket_text(conn, data);
        free(data);
    }
}

/* ======================= 静态文件处理 ======================= */

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
        write_response(conn, content, fsize);
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
        write_response(conn, msg, strlen(msg));
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

        if (conn.ssl) {
            SSL_set_fd(conn.ssl, conn.client_fd);
            if (SSL_accept(conn.ssl) <= 0) {
                SSL_free(conn.ssl);
                close(conn.client_fd);
                continue;
            }
        }

        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int n = read_request(&conn, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            if (conn.ssl) SSL_free(conn.ssl);
            close(conn.client_fd);
            continue;
        }
        printf("%s\n", buffer);

        char *path = prase_path(buffer);
        if (path == NULL) {
            if (conn.ssl) SSL_free(conn.ssl);
            close(conn.client_fd);
            continue;
        }

        /* 检测是否为 WebSocket 升级请求 */
        char *upgrade = get_header_value(buffer, "Upgrade");
        int is_ws = 0;
        if (upgrade) {
            if (strcasecmp(upgrade, "websocket") == 0)
                is_ws = 1;
            free(upgrade);
        }

        if (is_ws) {
            /* WebSocket 握手 + 进入帧循环 */
            if (handle_websocket_upgrade(&conn, buffer) == 0) {
                handle_websocket_echo(&conn);
            }
        } else if (strcmp(path, "/ws") == 0) {
            /* 也接受 /ws 路径的 WebSocket 连接 */
            if (handle_websocket_upgrade(&conn, buffer) == 0) {
                handle_websocket_echo(&conn);
            }
        } else {
            /* 普通 HTTP 处理 */
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
        }

        free(path);
        if (conn.ssl) SSL_free(conn.ssl);
        close(conn.client_fd);
    }
    return NULL;
}

/* ======================= 监听服务 ======================= */

struct accept_param {
    int server_fd;
    int is_https;
};

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

    init_openssl();
    ssl_ctx = create_ssl_ctx();
    if (!ssl_ctx) printf("SSL init failed, HTTP only\n");

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&pool[i], NULL, worker, &ctx);
    }

    int http_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(http_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9090);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(http_fd, 10);
    printf("HTTP  server on http://localhost:9090\n");

    int https_fd = -1;
    if (ssl_ctx) {
        https_fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(https_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9443);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(https_fd, (struct sockaddr *)&addr, sizeof(addr));
        listen(https_fd, 10);
        printf("HTTPS server on https://localhost:9443\n");
    }

    pthread_t http_thread, https_thread;
    struct accept_param *hp = malloc(sizeof(struct accept_param));
    hp->server_fd = http_fd; hp->is_https = 0;
    pthread_create(&http_thread, NULL, accept_loop, hp);

    if (https_fd != -1) {
        struct accept_param *hsp = malloc(sizeof(struct accept_param));
        hsp->server_fd = https_fd; hsp->is_https = 1;
        pthread_create(&https_thread, NULL, accept_loop, hsp);
        pthread_join(https_thread, NULL);
    }

    pthread_join(http_thread, NULL);
    close(http_fd);
    if (https_fd != -1) close(https_fd);
    if (ssl_ctx) SSL_CTX_free(ssl_ctx);
    return 0;
}
