# tcp_socket_basics — TCP Socket 编程模型

## 概述

TCP Socket 是客户端-服务器通信的最基础模型：**服务器监听一个端口，等待客户端连接，连接建立后双方收发数据。**

## 服务器端完整流程

```c
socket()  ── 创建 socket 文件描述符
    │
bind()    ── 绑定 IP 地址和端口号
    │
listen()  ── 将 socket 设为被动监听模式
    │
accept()  ── 阻塞等待客户端连接（返回新的 fd）
    │
recv()    ── 接收客户端发送的数据
send()    ── 向客户端发送数据
    │
close()   ── 关闭连接
```

## bind 详解

将一个 socket 绑定到特定的**IP 地址 + 端口号**：

```c
struct sockaddr_in addr;
addr.sin_family = AF_INET;           // IPv4
addr.sin_port = htons(9090);         // 端口号（网络字节序）
addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
```

### htons() — 字节序转换

```c
uint16_t htons(uint16_t hostshort);  // Host TO Network Short
uint32_t htonl(uint32_t hostlong);   // Host TO Network Long
```

- **主机字节序**：x86 CPU 用**小端**（低位字节在低地址）
- **网络字节序**：网络传输用**大端**（高位字节在低地址）
- **必须转换**，否则端口号会解释错误

## listen 详解

```c
int listen(int sockfd, int backlog);
```

- `sockfd`：要监听的 socket
- `backlog`：连接请求队列的最大长度（例：10）

## accept 详解

```c
socklen_t addr_len = sizeof(client_addr);
int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
```

- 返回**新的 socket 描述符**，用于与这个客户端通信
- `server_fd` 保留继续 accept 新连接
- **第三个参数必须传指针**（`&addr_len`），不能传值（`sizeof`）

## 阻塞 I/O

默认情况下，这些函数会**阻塞**（程序卡住等待）：

| 函数 | 阻塞条件 |
|------|---------|
| `accept()` | 没有客户端连接 |
| `recv()` | 客户端没有发数据 |
| `send()` | 发送缓冲区满（通常不阻塞） |

## 关联页面

- [[socket_fd]]
