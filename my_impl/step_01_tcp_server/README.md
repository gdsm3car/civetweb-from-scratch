# 里程碑 01：TCP Server

> **目标**：用 C 语言实现一个最原始的 TCP server。
> 理解 socket → bind → listen → accept 这一条完整的链路。

## 要回答的问题

1. 什么是 socket？它是一个文件描述符吗？
2. `bind` 为什么需要端口和 IP？
3. `listen` 的 backlog 参数是什么意思？
4. `accept` 返回的是什么？阻塞还是非阻塞？
5. 怎么让客户端发来的数据？怎么回复？

## 要求

- 使用纯 C 语言，不依赖任何第三方库
- Linux 系统调用（`socket`, `bind`, `listen`, `accept`, `read`, `write`）
- 单线程，一次只能处理一个连接
- 客户端发送什么，服务器打印什么，然后回复 "Hello from my server!"

## 编译与运行

```bash
cd step_01_tcp_server
make
./server
# 另一个终端
curl http://localhost:8080
```

## 完成后

在 `/wiki/concepts/` 中创建页面总结：TCP 协议、socket 编程模型、阻塞 I/O

## 对应的 Wiki 页面

- [[tcp_socket_basics]]
