# /my_impl — 从零复现 CivetWeb

> 这是一个递进式的学习工程。每个 step 目录是一个**独立可编译的里程碑**，逐步逼近 CivetWeb 的功能。

## 🧩 里程碑总览

| # | 目录 | 目标 | 前置知识 |
|---|------|------|---------|
| 00 | `step_00_preview/` | 编译 CivetWeb，跑起来观察行为 | C 语言基础 |
| 01 | `step_01_tcp_server/` | 实现一个 TCP server（socket→bind→listen→accept） | 无 |
| 02 | `step_02_http_parser/` | 解析 HTTP GET 请求报文 | 里程碑 01 |
| 03 | `step_03_static_file/` | 从磁盘读取文件，通过 HTTP 返回 | 里程碑 02 |
| 04 | `step_04_threadpool/` | 引入线程池处理并发连接 | 里程碑 03 |
| 05 | `step_05_callback/` | 用户注册回调函数处理请求 | 里程碑 04 |
| 06 | `step_06_tls/` | 集成 TLS 实现 HTTPS | 里程碑 04 |
| 07 | `step_07_websocket/` | 实现 WebSocket 握手与通信 | 里程碑 05 |
| 08 | `step_08_advanced/` | 路由、CGI、Lua 等进阶 | 里程碑 07 |

## 📐 设计原则

1. **每个 step 都独立可编译** — `cd step_xx && make && ./server`
2. **递进不倒退** — step N 包含 step N-1 的全部功能 + 新增特性
3. **先 Why 后 How** — 每次动手前，先理解这个阶段要解决什么问题
4. **不照搬 CivetWeb** — 用自己的思路实现，遇到瓶颈再参考 `/raw`

## 🔁 学习方式

在每个 step 的对话中，我遵循 SETUP 工作流：

```
分析(你遇到的问题 / /raw 对照) 
  → 计划(确认方案) 
    → 执行(写代码) 
      → 关联(沉淀到 /wiki) 
        → 日志(记录到 log.md)
```

## 🚀 当前进度

```
里程碑 00 [⏳]  里程碑 01 [    ]  里程碑 02 [    ]  里程碑 03 [    ]
里程碑 04 [    ]  里程碑 05 [    ]  里程碑 06 [    ]  里程碑 07 [    ]
里程碑 08 [    ]
```
