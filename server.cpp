#include "./include/server.h"

#define MAX_FD 65535 // 最大的文件描述符个数


// 添加文件描述符到epoll中
extern void AddFD(int epollfd, int fd, bool one_shot);
// 删除epoll中的文件描述符
extern void RemoveFD(int epollfd, int fd);
// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下次可读
extern void ModFD(int epollfd, int fd, int ev);

// 添加信号捕捉
void addsig(int sig, void(*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); // 将所有信号加入信号集，表示系统支持的信号均为有效信号
    sigaction(sig, &sa, nullptr); // 添加信号捕捉以及信号处理函数
}


Server::Server() : tp(nullptr), tm(nullptr) {}

Server::~Server() {}

void Server::InitThreadPool() {
    try
    {
        tp = new threadpool<HttpConn>;
    }
    catch(...)
    {
        exit(-1);
    }
}

void Server::InitDBConn() {
    DBConnPool::Init("localhost", "root", "123456", "tiny_webserver", 3306, 10);
    HttpConn::coon_pool = DBConnPool::GetInstance();
}

void Server::InitTimer() {
    TimerManager::Init(60);   // 初始化定时器保活时间，单位s
    tm = TimerManager::GetInstance();
}

void Server::InitLog(bool async) {
    Log::Init("server", Log::DEBUG, 5000, 1000, true);
    log = Log::GetInstance();
}

void Server::Run(int port) {
    addsig(SIGPIPE, SIG_IGN); // 忽略
    addsig(SIGALRM, TimerManager::Tick); // 添加接受定时器的ALARM信号的回调函数
    
    struct itimerval it;
    it.it_value.tv_sec = 10; // 第一次到期的时间，即1s后到期
    it.it_value.tv_usec = 0;
    it.it_interval.tv_sec = 10; // 第一次到期的时间后用于重置的时间，即重置it_value为it_interval
    it.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &it, nullptr); // 间隔1s发送一次ALARM信号

    // 创建一个数组用于保存所有的客户端信息
    HttpConn *users = new HttpConn[MAX_FD]; 
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
    AddFD(epfd, lfd, false);   
    HttpConn::m_epollfd = epfd; 
    TimerManager::m_epollfd = epfd;
    int maxevents = 1024;                                 
    struct epoll_event events[1024];
    memset(events, 0, sizeof(events));
    while (1) {
        int fd_cnt = epoll_wait(epfd, events, maxevents, -1);
        for (int i = 0; i < fd_cnt; ++i) {
            int sockfd = events[i].data.fd;
            // 如果服务器监听连接的I/O就绪
            if (sockfd == lfd) {
                int cfd = 0;
                sockaddr_in c_addr;
                socklen_t len = sizeof(c_addr); 
                while ((cfd = accept(lfd, (struct sockaddr *)&c_addr, &len)) != -1) {
                    if (HttpConn::m_user_count >= MAX_FD) {
                        // 目前连接数已满，无法接受更多连接
                        printf("connection number overload\n");
                        close(cfd);
                        continue;
                    }
                    // 在数组中记录客户端连接信息
                    users[cfd].Init(cfd);
                    tm->AddTimer(users + cfd, c_addr);
                    // 输出客户端信息
                    char clientIP[16];
                    inet_ntop(AF_INET, &c_addr.sin_addr.s_addr, clientIP, sizeof(clientIP));
                    unsigned short clientPort = ntohs(c_addr.sin_port);
                    printf("建立连接 - client ip is %s, port is %d\n", clientIP, clientPort);
                    LOG_INFO("[client] %s:%d", clientIP, clientPort);
                }
                if (cfd == -1) {
                    // 产生中断或者读取完fd数据时不处理
                    if (errno == EINTR || errno == EAGAIN) {
                        continue;
                    }
                    perror("accept");
                    exit(-1);
                }
            }
            // 异常事件，清理定时器并关闭文件描述符
            else if (events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
                tm->DelTimer(sockfd);
            }
            else if (events[i].events & EPOLLIN) {
                // 一次性读取socket数据成功
                if (users[sockfd].Read()) {
                    tm->AdjustTimer(sockfd);
                    tp->append_task(users + sockfd);
                }
                else {
                    tm->DelTimer(sockfd);
                }
            }
            else if (events[i].events & EPOLLOUT) {
                // 一次性读取socket数据成功
                if (!users[sockfd].Write()) {
                    tm->DelTimer(sockfd);
                }
            }
        }
    }
}