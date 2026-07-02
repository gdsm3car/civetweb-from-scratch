# 里程碑 05：回调机制

> **目标**：模仿 CivetWeb 的 mg_callbacks，设计用户注册回调来处理请求。

## 前置

完成里程碑 04

## 任务

1. 设计一个回调注册接口，让用户可以自定义请求处理函数
2. 服务器收到请求后，遍历已注册的回调，找到对应的 handler
3. 如果没找到匹配的回调，走默认的静态文件处理

## 回调接口雏形

```c
typedef int (*request_handler)(struct mg_connection *conn, void *user_data);

void mg_set_request_handler(struct mg_context *ctx, 
                            const char *uri_pattern,
                            request_handler handler,
                            void *user_data);
```
