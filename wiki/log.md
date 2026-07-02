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
