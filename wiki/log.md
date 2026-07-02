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
