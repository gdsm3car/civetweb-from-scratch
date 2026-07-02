# socket_fd — Socket 文件描述符

## 是什么

socket 是操作系统提供的一种 **网络通信接口**。在 Linux 中，socket 以**文件描述符**（一个整数）的形式存在。

## 核心特性

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
// fd 是一个整数，比如 3
```

- 与文件描述符本质相同，可以用 `read()`/`write()` 操作
- 只是操作的底层对象从"磁盘文件"变成了"网络连接"

## 创建函数

| 参数 | 值 | 含义 |
|------|-----|------|
| `domain` | `AF_INET` | IPv4 协议 |
| `type` | `SOCK_STREAM` | 面向连接的 TCP 流 |
| `protocol` | `0` | 自动选择协议 |

## 生命周期

```
socket() → bind() → listen() → accept() → recv()/send() → close()
  创建      绑定      监听      接受连接    收发数据     关闭
```

## 关键区分

| 描述符 | 角色 | 用途 |
|--------|------|------|
| `server_fd` | 总机/接线员 | 只做 accept（接听新的来电） |
| `client_fd` | 专线 | 做 recv/send（与具体客户端通话） |

## 关联页面

- [[tcp_socket_basics]]
