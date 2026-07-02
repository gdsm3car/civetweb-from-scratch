# entities — 核心实体目录

> 记录 CivetWeb 和 /my_impl 复现中的核心实体。
> 格式：每个实体一个文件，描述其数据结构、职责、生命周期。

## 待创建清单（按学习顺序）

- [ ] **socket_fd** — socket 文件描述符
- [ ] **http_request** — HTTP 请求结构体（方法、路径、头）
- [ ] **http_response** — HTTP 响应结构体（状态码、头、体）
- [ ] **connection** — 连接对象（socket fd、请求/响应缓冲区）
- [ ] **thread_pool** — 线程池（线程数组、任务队列、锁）
- [ ] **server_context** — 服务器上下文（端口、根目录、配置）
- [ ] **callback_handler** — 回调处理器（URL 模式匹配、处理函数）
- [ ] **tls_context** — TLS 上下文（SSL_CTX、证书）
- [ ] **websocket_frame** — WebSocket 帧结构
