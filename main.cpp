#include <iostream>
#include <cstring>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "./include/threadpool.h"
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>
#include <signal.h>
#include "./include/http_conn.h"

#define MAX_FD 65535 // 最大的文件描述符个数

// 添加信号捕捉
void addsig(int sig, void(*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); // 将所有信号加入信号集，表示系统支持的信号均为有效信号
    sigaction(sig, &sa, nullptr); // 添加信号捕捉已经信号处理函数
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
// 删除epoll中的文件描述符
extern void removefd(int epollfd, int fd);
// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下次可读
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char **argv) {
    if (argc <= 1) {
        printf("请按照如下格式运行程序: %s port_number\n", basename(argv[0]));
        exit(-1);
    }
    int port = atoi(argv[1]);
    addsig(SIGPIPE, SIG_IGN); // 忽略
    // 初始化线程池
    threadpool<http_conn> *tp = nullptr;
    try
    {
        tp = new threadpool<http_conn>;
    }
    catch(...)
    {
        exit(-1);
    }
    
    // 创建一个数组用于保存所有的客户端信息
    http_conn *users = new http_conn[MAX_FD];

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        exit(-1);
    }
    // 设置端口复用
    int reuse = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    sockaddr_in s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = INADDR_ANY;
    int ret = bind(lfd, (sockaddr *)&s_addr, sizeof(s_addr));
    if (ret == -1) {
        perror("bind");
        exit(-1);
    }
    ret = listen(lfd, 100);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }
    // 创建epoll实例
    int epfd = epoll_create(4);
    if (epfd == -1) {
        perror("epoll_create");
        exit(-1);
    }
    // 添加服务端监听文件描述符
    addfd(epfd, lfd, false);   
    http_conn::m_epollfd = epfd; 
    int maxevents = 1024;                                 
    struct epoll_event events[1024];
    memset(events, 0, sizeof(events));
    while (1) {
        int fd_cnt = epoll_wait(epfd, events, maxevents, -1);
        for (int i = 0; i < fd_cnt; ++i) {
            int sockfd = events[i].data.fd;
            // 如果服务器监听连接的I/O就绪
            if (sockfd == lfd) {
                sockaddr_in c_addr;
                socklen_t len = sizeof(c_addr);
                int cfd = accept(lfd, (struct sockaddr *)&c_addr, &len);
                if (cfd == -1) {
                    // 产生中断时不处理
                    if (errno == EINTR) {
                        continue;
                    }
                    perror("accept");
                    exit(-1);
                }
                if (http_conn::m_user_count >= MAX_FD) {
                    // 目前连接数已满，无法接受更多连接
                    printf("connection number overload\n");
                    close(cfd);
                    continue;
                }
                
                // 在数组中记录客户端连接信息
                users[cfd].init(cfd);
                // 输出客户端信息
                char clientIP[16];
                inet_ntop(AF_INET, &c_addr.sin_addr.s_addr, clientIP, sizeof(clientIP));
                unsigned short clientPort = ntohs(c_addr.sin_port);
                printf("client ip is %s, port is %d\n", clientIP, clientPort);

            }
            // 异常事件，关闭文件描述符
            else if (events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN) {
                // 一次性读取socket数据成功
                if (users[sockfd].read()) {
                    tp->append_task(users + sockfd);
                }
                else {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT) {
                // 一次性读取socket数据成功
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }
    return 0;
}