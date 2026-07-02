# 里程碑 02：HTTP 协议解析

> **目标**：解析 HTTP GET 请求报文，理解请求行、请求头、空行的含义。

## 前置

完成里程碑 01

## 任务

在 TCP server 基础上，增加 HTTP 解析能力：

1. 读取客户端发来的原始 HTTP 请求
2. 解析请求行（`GET /path HTTP/1.1`）
3. 解析请求头（`Host: ...`、`User-Agent: ...`）
4. 根据不同的路径返回不同的响应
   - `/` → `200 OK` + "Welcome"
   - `/about` → `200 OK` + "About page"
   - 其他 → `404 Not Found`
5. 响应必须包含正确的 HTTP 状态行和头

## HTTP 响应格式

```
HTTP/1.1 200 OK\r\n
Content-Type: text/plain\r\n
Content-Length: 7\r\n
\r\n
Welcome
```
