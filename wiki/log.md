## [2026-07-02] - Session 1：项目初始化 & 选型

### 本次目标
- 阅读 SETUP 方法论，初始化项目学习结构
- 在 GitHub 上寻找合适的 C/C++ 嵌入式 Web 后端项目

### 重点记录
- 选型结果：**CivetWeb** — 嵌入式 C/C++ Web 服务器（MIT 许可证）
- 确定了从零递进式复现的学习策略，不直接读 CivetWeb 源码
- 创建了 8 个递进式里程碑（TCP → HTTP → 文件 → 线程 → 回调 → TLS → WebSocket → 进阶）
- 克隆 CivetWeb 仓库到 `/raw/` 作为参考答案

### 下一步
- 开始里程碑 00

### 关联页面
- [[index]]
- [[SETUP]]

---

## [2026-07-02] - 里程碑 00：编译 + 观察 CivetWeb

### 本次目标
- 编译 CivetWeb 并观察其行为

### 重点记录
- 顺利编译，但发现了 CivetWeb 1.17 源码中的一个**语法 bug**
- Bug 原因：变量 `cl` 被误删，导致 4 处编译错误
- 修复后编译通过，生成 199KB 的 ELF 可执行文件
- 运行 `./civetweb -document_root . -listening_ports 8080`，成功启动
- 用 curl 做了 3 个实验：正常访问 / 、查看完整 HTTP 报文、访问不存在的路径

### 观察结果
- CivetWeb 默认开启 **Directory Indexing**（自动生成文件列表）
- 使用 **HTTP 短连接**（`Connection: close`）
- 占用内存极低（RSS ~2MB）
- 内置 404 错误页面

### 发现的问题
- **Bug 位置**: `src/civetweb.c` 中 `get_request()` 函数
- **Bug 症状**:
  - `if` 语句结构错误，`&&` 悬空（第 19210-19211 行）
  - 使用了未声明的变量 `cl`（第 19227 行）
  - 变量 `h_zip` 产生 unused warning
- **根因**：变量声明被误删，`h_chunk` 和 `h_len` 应取代原来的 `cl`
- **修复**：四处 `cl` → 替换为 `h_chunk`（2处）和 `h_len`（2处）
- **教训**：即便是成熟的开源项目也有语法级别的 bug

### 下一步
- 开始里程碑 01：从零手写 TCP Server
- 用 C 的 socket API 实现 `bind → listen → accept → read → write`

### 关联页面
- [[index]]
- [[step_00_preview_notes]]

---

## [2026-07-02] - 里程碑 01：TCP Server

### 本次目标
- 从零手写一个 TCP Server，实现 socket → bind → listen → accept → recv → send → close

### 重点记录
- 成功编写了 `server.c`，监听 9090 端口
- 学习和使用了 5 个关键系统调用：`socket`, `bind`, `listen`, `accept`, `close`
- 使用了 `recv()` / `send()` 进行数据收发
- 理解了 `struct sockaddr_in`、`htons()`、`htonl()`、`socklen_t` 的用法

### 发现的 Bug（自检修正）
1. **重复定义 struct sockaddr_in** — 头文件 `<netinet/in.h>` 已定义，自己又写了一遍
2. **accept 第三参数** — 传了 `sizeof()` 值而不是 `&addr_len` 指针
3. **recv/send 用错 socket** — 误用 `server_fd` 代替 `client_fd` 收发数据
4. **send 长度** — `sizeof(msg)` 发送 1024 字节，应为 `strlen(msg)`
5. **accept 错误处理粘贴错误** — 残留了 `listen()` 的判断代码

### 验证结果
- ✅ 编译通过（仅剩 unused variable 警告）
- ✅ curl 能连接到端口 9090，成功发送 HTTP 请求报文
- ⚠️ curl 报 `HTTP/0.9 when not allowed` — **预期行为**，因服务器未返回 HTTP 格式响应（下一阶段解决）
- ✅ `nc` 测试可达（TCP 收发正常）
- ✅ 服务器处理完一个连接后正常退出（单次模式，无循环）

### 关键认识
- socket 返回的文件描述符是一个整数，与文件描述符概念一致
- `server_fd` 是"总机"，`client_fd` 是"通话专线"
- curl 是 HTTP 客户端，不是纯 TCP 工具 → 需要 HTTP 协议解析才能正确通信

### 下一步
- 开始里程碑 02：HTTP 协议解析 — 让服务器读懂 `GET / HTTP/1.1`，回复正确格式的 HTTP 响应

### 关联页面
- [[index]]
- [[tcp_socket_basics]]
- [[socket_fd]]
- [[step_01_tcp_server]]

---

## [2026-07-02] - 里程碑 02：HTTP 协议解析

### 本次目标
- 实现 HTTP 请求解析，根据路径返回不同响应
- 回复符合 HTTP 格式的响应报文

### 重点记录
- 手写 `parse_path()` 函数，从 `"GET /path HTTP/1.1"` 中提取路径
- 使用 `sprintf()` 拼接 HTTP 响应字符串（状态行 + 头部 + 空行 + 体）
- 引入 `while(1)` 循环，服务器不再处理完一个请求就退出
- 封装了 `send_404()` 函数
- curl 测试全部通过，不再报 `HTTP/0.9` 错误

### 验证结果
| 路径 | 状态码 | 响应体 |
|------|--------|--------|
| `/` | `200 OK` | hello,world |
| `/about` | `404 Not Found` | HTML 404 页面 |
| `/nonexist` | `404 Not Found` | HTML 404 页面 |

### 待改进
- 函数名 `prase_path` 拼写错误（应为 `parse_path`）
- `bytes_read` 变量未使用
- `addr_len` 建议每次循环重置

### 关键技术点
- HTTP 报文格式：请求行/状态行 → 头部 → 空行 → 体
- `Content-Length` 头必须精确匹配响应体字节数
- 不同状态码对应不同语义（200/404）

### 下一步
- 开始里程碑 03：静态文件服务 — 从磁盘读取文件，自动设置 Content-Type，实现完整的静态 Web 服务器

### 关联页面
- [[index]]
- [[http_protocol]]

---

## [2026-07-02] - 里程碑 03：静态文件服务

### 本次目标
- 从磁盘读取文件，通过 HTTP 返回，实现完整的静态 Web 服务器

### 重点记录
- 实现了 `get_mime_type()` — 根据文件扩展名返回 Content-Type
- 实现了文件 I/O 流程：`fopen` → `fseek`/`ftell`(算大小) → `fread` → `fclose`
- **重要认识**：文件是二进制数据，HTTP 头部和响应体必须分开 `send`
- 响应体用 `malloc`/`fread` 读取，用 `send` 单独发送（不是拼在 `sprintf` 里）

### 自检修正的 Bug
1. `close(content)` 改为 `free(content)` — close 是关文件描述符，free 才是释放内存
2. 释放时机移到 send 之后 — use-after-free
3. `get_mime_type` 返回值加 `const`
4. 增加 `path == NULL` 的保护判断

### 验证结果
| 请求 | 状态码 | Content-Type | 结果 |
|------|--------|-------------|------|
| `/index.html` | 200 | text/html | ✅ |
| `/about.html` | 200 | text/html | ✅ |
| `/style.css` | 200 | text/css | ✅ |
| `/nonexist.html` | 404 | text/html | ✅ |

### 关键技术点
- 二进制文件必须用 `"rb"` 模式打开
- `Content-Length` 必须是精确的字节数（`ftell` 获取）
- 头部用 `snprintf` 拼成字符串，体用 `send` 直接发二进制数据
- `strrchr` 定位文件扩展名，`strcasecmp` 做大小写无关比较

### 下一步
- 开始里程碑 04：线程池 — 解决单线程服务器"一次只能服务一个客户端"的问题

### 关联页面
- [[index]]
- [[file_io]]
- [[http_protocol]]

---

## [2026-07-04] - 里程碑 04：线程池

### 本次目标
- 引入 pthread 线程池，解决单线程服务器阻塞问题

### 重点记录
- 实现了 4 线程的线程池，主线程只负责 accept 和派发
- 使用 `pthread_mutex_t` 保护任务队列的并发访问
- 使用 `pthread_cond_t` 实现任务通知（无任务时线程休眠）
- 用 `#define` 代替 `const int` 定义数组大小（C 语言限制）

### 并发对比测试结果

| 场景 | step 03（单线程） | step 04（线程池） |
|------|------------------|-----------------|
| 慢客户端阻塞时并发请求 | 4.47s ❌ | 0.001s ✅ |

### 自检修正的问题
1. `const int` 不能用作 C 语言数组长度 → 改为 `#define`
2. `worker` 函数 `arg` 参数未使用 → 添加 `(void)arg` 消警告

### 关键技术点
- `pthread_create` / `pthread_mutex_lock` / `pthread_cond_wait` 的使用
- 条件变量必须和互斥锁配合使用
- `while` 而非 `if` 判断条件（防止虚假唤醒）
- 编译时加 `-lpthread` 链接线程库
- 任务队列暂无敌保护检查（后续可优化）

### 下一步
- 开始里程碑 05：回调机制 — 模仿 CivetWeb 的 mg_callbacks，让用户注册 handler

### 关联页面
- [[index]]
- [[thread_pool]]
- [[pthread_basics]]

---

## [2026-07-04] - 里程碑 05：回调机制

### 本次目标
- 模仿 CivetWeb 的 mg_callbacks，设计可注册的请求处理函数

### 重点记录
- 定义了回调核心类型：`request_handler`（函数指针类型）
- 实现了 `struct mg_context` 服务器上下文，统一管理回调表
- 实现了 `mg_set_request_handler()` 注册接口
- 用 `serve_static_file()` 抽取默认处理逻辑，与回调分离
- 添加了两个示例回调：`/api/hello` → JSON, `/api/echo` → plain text

### 代码结构变化
```
里程碑 04：                         里程碑 05：
main() {                           main() {
    socket/bind/listen                  // 先注册回调
    while(1) accept → 队列              mg_set_request_handler("/api/hello", ...)
    ↑ 所有逻辑写死在 worker 里          mg_set_request_handler("/api/echo", ...)
}                                       socket/bind/listen
                                    }
                                    worker() → 先查回调表
                                                → 有匹配 → 调用回调
                                                → 不匹配 → serve_static_file()
```

### 验证结果
| 请求 | 处理方式 | 状态码 | 结果 |
|------|---------|--------|------|
| `/api/hello` | 回调 | 200 | `{"message":"hello"}` ✅ |
| `/api/echo` | 回调 | 200 | "echo" ✅ |
| `/index.html` | 静态文件兜底 | 200 | HTML ✅ |
| `/notfound` | 静态文件兜底 | 404 | 404 页 ✅ |

### 关键技术点：函数指针
```c
typedef int (*request_handler)(struct mg_connection *conn, void *user_data);
//        ↑ 返回值  ↑ 参数列表                      ↑ 参数名
// 这个类型表示"指向一个函数的指针，该函数接收 conn 和 user_data，返回 int"
```

### 下一步
- 开始里程碑 06：HTTPS/TLS — 集成 OpenSSL，实现加密通信

### 关联页面
- [[index]]
- [[callback_design]]
- [[function_pointer]]

---

## [2026-07-05] - 里程碑 06：HTTPS/TLS

### 本次目标
- 集成 OpenSSL，实现 HTTPS 加密通信
- 同时支持 HTTP（9090）和 HTTPS（9443）双端口

### 重点记录
- 安装了 `libssl-dev` OpenSSL 开发库
- 生成了自签名证书（`cert.pem` + `key.pem`）
- 修改了代码架构：
  - `struct mg_connection` 新增 `SSL *ssl` 字段，区分 HTTP/HTTPS
  - `read_request()` / `write_response()` 统一封装，自动识别加密通道
  - 任务队列改用 `struct task` 携带 SSL 对象
  - 新增 `accept_loop()` 线程化，同时监听两个端口
- 代码改动集中在传输层，HTTP 解析、回调、静态文件**完全不变**

### 验证结果
| 测试 | 命令 | 结果 |
|------|------|------|
| HTTP 静态文件 | `curl http://localhost:9090/index.html` | ✅ |
| HTTPS 静态文件 | `curl -sk https://localhost:9443/index.html` | ✅ |
| HTTPS 回调 | `curl -sk https://localhost:9443/api/hello` | ✅ |
| HTTPS 404 | `curl -sk https://localhost:9443/nonexist` | ✅ |
| 加密详情 | TLSv1.3 / TLS_AES_256_GCM_SHA384 | ✅ |

### 关键技术点
- TLS 握手流程：ClientHello → ServerHello + Certificate → 密钥协商
- 非对称加密交换对称密钥 → 后续用对称加密（兼顾安全与性能）
- `SSL_read/SSL_write` 代替 `recv/send`，上层逻辑不受影响
- `SSL_new()` 在每个连接上创建，`SSL_CTX` 全局共享
- 编译时链接 `-lssl -lcrypto`
- 自签名证书用 `-k` 参数忽略验证

### 下一步
- 开始里程碑 07：WebSocket — 实现全双工通信

### 关联页面
- [[index]]
- [[tls_ssl]]
- [[openssl_basics]]

---

## [2026-07-06] - 里程碑 07：WebSocket

### 本次目标
- 实现 WebSocket 握手与帧通信
- 支持 Echo 回显功能

### 重点记录
- 实现了 `get_header_value()` — 从 HTTP 请求头中提取指定头字段
- 实现了 `handle_websocket_upgrade()` — 检测 Upgrade: websocket，计算 Sec-WebSocket-Accept（SHA1+Base64），返回 101
- 实现了 `read_websocket_frame()` — 解析 WebSocket 数据帧（opcode、MASK、扩展长度 126/127、掩码解码）
- 实现了 `send_websocket_frame()` — 发送帧（支持小/中/大三种长度编码）
- 实现了 `base64_encode()` — Base64 编码函数（用于 Accept 值计算）
- 新增 `/echo` 路径的 WebSocket echo 服务

### 关键技术点
- **握手**：`Sec-WebSocket-Accept = Base64(SHA1(key + "258EAFA5-E914-47DA-95CA-5AB9DC11B85B"))`
- **帧格式**：首字节 FIN+opcode，次字节 MASK+length，长度超过 125 用扩展字节，客户端帧必须掩码
- **掩码**：客户端用 4 字节掩码 XOR 每个 payload 字节，服务器端收到后同样方式解码
- **服务器帧**：不设置 MASK 位，不需要掩码

### 验证结果
```
握手: HTTP/1.1 101 Switching Protocols ✅
发送: "Hello WebSocket!"               ✅
回显: "Hello WebSocket!"               ✅
```

### 下一步
- 开始里程碑 08：进阶功能 — 路由前缀匹配、CGI、路径穿越防护等

### 关联页面
- [[index]]
- [[websocket_protocol]]
