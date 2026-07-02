# 里程碑 00 观察记录

> 日期：2026-07-02
> 对照版本：CivetWeb v1.17

## 编译

- 初始 `make` 报错，需要修复源码中的 4 处 bug
- 修复后编译成功，生成 199KB ELF 可执行文件
- 依赖：仅需 `gcc`, `make`, `libpthread`（Linux 自带）

## 运行行为

### 命令
```bash
./civetweb -document_root . -listening_ports 8080
```

### 实验 1：`curl http://localhost:8080`
- 返回 HTML **目录索引**，列出文档根目录的所有文件和子目录
- CivetWeb 内置了 Directory Indexing 功能

### 实验 2：`curl -v http://localhost:8080`
请求：
```
GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/7.81.0
```
响应：
```
HTTP/1.1 200 OK
Cache-Control: max-age=3600
Content-Type: text/html; charset=utf-8
Date: Thu, 02 Jul 2026 05:47:46 GMT
Connection: close
```

### 实验 3：`curl -v http://localhost:8080/something`
- 返回 `404 Not Found`，body 内容为 "Error 404: Not Found"

### 实验 4：进程信息
```
dawn 14218 0.0 0.0 265672 2048 pts/4 Sl+ 13:44 0:00 ./civetweb ...
```
- VSZ: 265672 (虚拟内存)
- RSS: 2048 (实际物理内存 ~2MB)
- 非常轻量

## 关键发现

| 发现 | 说明 |
|------|------|
| 目录索引 | 无 index.html 时自动生成目录列表 |
| 短连接 | `Connection: close`，每次请求后断开 |
| 轻量级 | 占用仅 ~2MB 物理内存 |
| 命令行参数 | `-document_root` 指定网站根目录，`-listening_ports` 指定端口 |
| 404 处理 | 有内置的错误页面 |
