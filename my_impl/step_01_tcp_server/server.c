#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET (IPv4)
    in_port_t      sin_port;     // 端口号 (要用 htons 转换)
    struct in_addr sin_addr;     // IP 地址
};*/ //定义的格式

int main() {
    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); //File Descriptor
    if(server_fd==-1)
    {
        printf("socket create errror");
        exit(EXIT_FAILURE);
    }
    // 2. bind 到端口 9090
    struct sockaddr_in server_addr,client_addr;
    memset(&server_addr, 0, sizeof(server_addr)); //清空内存
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr))==-1)
    {
        printf("bind errror");
        exit(EXIT_FAILURE);
    }
    // 3. listen 开始监听
    if(listen(server_fd,10)==-1)
    {
        printf("listen errror");
        exit(EXIT_FAILURE);
    }
    // 4. accept 等待客户端连接
    socklen_t addr_len = sizeof(client_addr);
    while(1)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if(client_fd==-1)
        {
            printf("accept errror");
            exit(EXIT_FAILURE);
        }
        // 5. read 客户端发来的数据
        char buffer[1024];
        ssize_t bytes_read;
        memset(buffer, 0, sizeof(buffer));

        if(recv(client_fd,buffer,sizeof(buffer),0)==-1)
        {
            printf("recv errror");
            exit(EXIT_FAILURE);
        }

        // 6. write 回复 "Hello from my server!"
        char msg[1024] = "Hello from my server!";
        if(send(client_fd,msg,strlen(msg),0)==-1)
        {
            printf("send errror");
            exit(EXIT_FAILURE);
        }
        // 7. close 连接
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
