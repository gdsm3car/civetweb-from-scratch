
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

/*struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET (IPv4)
    in_port_t      sin_port;     // 端口号 (要用 htons 转换)
    struct in_addr sin_addr;     // IP 地址
};*/ //定义的格式

//加入线程池

#define THREAD_POOL_SIZE 4
#define QUEUE_SIZE 64
int que_size = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t pool[THREAD_POOL_SIZE];
int task_queue[QUEUE_SIZE];

const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";

    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".css") == 0)
        return "text/css";
    if (strcasecmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcasecmp(ext, ".png") == 0)
        return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcasecmp(ext, ".ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, ".json") == 0)
        return "application/json";

    return "application/octet-stream";
}


char * prase_path(char * buffer)
{
    int i = 0;
    while(buffer[i]!=' '&&buffer[i]!='\0'&&buffer[i]!='\r')
        i++; 
    if (buffer[i] == '\0'||buffer[i]=='\r') return NULL; //格式错误
    int len = 0;
    i++; //跳向起点
    while (buffer[i + len] != ' '&&buffer[i + len] != '\0'&&buffer[i + len]!='\r')
        len++;
    if (len == 0) return NULL;
    char * path = malloc(len + 1); //分配内存并且拷贝
    memcpy(path, &buffer[i], len);
    path[len] = '\0';
    return path;
}

int send_404(int client_fd) {
    const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
    char msg[1024];
    
    sprintf(msg,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(body), body);
            
    return send(client_fd, msg, strlen(msg), 0);
}

void * worker(void * arg)
{
    while(1)
    {
        pthread_mutex_lock(&mutex);
        while(que_size==0)
        {
            pthread_cond_wait(&cond, &mutex);
        }
        int client_fd = task_queue[--que_size];  // 取任务
        pthread_mutex_unlock(&mutex);
        // 5. read 客户端发来的数据
        char buffer[1024];
        ssize_t bytes_read;
        memset(buffer, 0, sizeof(buffer));
        if(recv(client_fd,buffer,sizeof(buffer)-1,0)==-1)
        {
            printf("recv errror");
            exit(EXIT_FAILURE);
        }
        printf("%s\n",buffer);
        char *path = prase_path(buffer);
        if (path == NULL) 
        {
            send_404(client_fd);
            close(client_fd);
            continue;
        }

        char *root = "www";
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", root, path);
        FILE *fp = fopen(filepath, "rb");
        char msg[1024];
        int sendflag;
        if(fp!=NULL)
        {
            //文件处理
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char * const content = malloc(fsize);
            fread(content, 1, fsize, fp);
            fclose(fp);
            //拼出http相应
            const char *con_type = get_mime_type(path);
            char header[512];
            int header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: %s\r\n"
                                      "Content-Length: %ld\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      con_type, fsize);
            sendflag=send(client_fd, header, header_len, 0);
            sendflag=send(client_fd, content, fsize, 0);
            free(content);
        }
        else if (path!=NULL&&strcmp(path, "/") == 0)
        {
            sprintf(msg,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n" // 空行
            "hello,world");
            sendflag = send(client_fd, msg, strlen(msg), 0);
        }
        else
        {
            sendflag = send_404(client_fd);
        }
        if(sendflag==-1)
        {
            printf("send errror");
            exit(EXIT_FAILURE);
        }
        // 7. close 连接
        close(client_fd);
        free(path);
    }
}

int main() {
    for (int i = 0; i < THREAD_POOL_SIZE;i++)
    {
        pthread_create(&pool[i], NULL, worker, NULL);
    }

    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // File Descriptor
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
        else
        {
            pthread_mutex_lock(&mutex);
            task_queue[que_size++] = client_fd;
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&cond);
        }
    }
    close(server_fd);
    return 0;
}
