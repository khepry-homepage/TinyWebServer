#include "../include/server.h"

#include <memory>
namespace TinyWebServer {
#define MAX_FD 65535  // 最大的文件描述符个数

// 设置非阻塞IO
extern void SetNonBlock(int fd);

// 添加信号捕捉
void AddSig(int sig, void (*handler)(int)) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(
      &sa.sa_mask);  // 将所有信号加入信号集，表示系统支持的信号均为有效信号
  sigaction(sig, &sa, nullptr);  // 添加信号捕捉以及信号处理函数
}

bool Server::server_run_ = false;

Server::Server(const int &reactor_count)
    : reactor_count_(reactor_count),
      reactors_(reactor_count),
      start_index_(0),
      buf_(1) {
  stopfd_ = eventfd(0, EFD_NONBLOCK);
  threads_ = std::make_unique<pthread_t[]>(reactor_count);
  for (auto &ptr : reactors_) {
    ptr = std::make_unique<SubReactor>();
  }
  // 创建reactor_count_个线程
  for (int i = 0; i < reactor_count; ++i) {
    LOG_DEBUG("create the %dth reactor", i);
    if (pthread_create(&threads_[i], nullptr, Reactor::Worker,
                       reactors_[i].get()) != 0) {
      throw std::exception();
    }
  }
  server_run_ = true;
}

Server::~Server() {
  for (const auto &reactor_ptr : reactors_) {
    reactor_ptr->NotifyDestroy();
  }
  for (int i = 0; i < reactor_count_; ++i) {
    pthread_join(threads_[i], nullptr);
  }
  reactors_.clear();
}

void Server::InitThreadPool() { ThreadPool<SmartHttpConn>::Init(1000); }

void Server::InitDBConn() {
  DBConnPool::Init("localhost", "root", "123456", "tiny_webserver", 3306, 10);
  HttpConn::coon_pool_ = DBConnPool::GetInstance();
}

void Server::InitTimer() {
  TimerManager::Init(10, 5);  // 初始化定时器保活时间，单位s
}

void Server::InitLog(bool async) {
  Log::Init("server", Log::DEBUG, 10, 1000, true);
  log_ = Log::GetInstance();
}

void Server::Run(int port) {
  AddSig(SIGPIPE, SIG_IGN);             // 忽略
  AddSig(SIGINT, Server::CloseServer);  // 接受2号信号则关闭服务器
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
  SetNonBlock(lfd);
  pollfd fds[2];
  fds[0].fd = lfd;
  fds[1].fd = stopfd_;
  fds[0].events = POLL_IN;
  fds[1].events = POLL_IN;
  int nfds = 2;
  while (server_run_) {
    int ret = poll(fds, nfds, -1);
    if (ret == -1 && errno != EINTR) {
      perror("poll");
      exit(-1);
    }
    for (int i = 0; i < nfds; ++i) {
      if (fds[i].revents != 0) {
        int fd = fds[i].fd;
        if (fd == lfd && (fds[i].revents & POLL_IN)) {
          int cfd = 0;
          sockaddr_in c_addr;
          socklen_t len = sizeof(c_addr);
          while ((cfd = accept(lfd, (struct sockaddr *)&c_addr, &len)) != -1) {
            if (HttpConn::user_count_ >= MAX_FD) {
              // 目前连接数已满，无法接受更多连接
              LOG_DEBUG("connect fail and can not accept overload connection");
              close(cfd);
              continue;
            }
            // 输出客户端信息
            char client_ip[16];
            inet_ntop(AF_INET, &c_addr.sin_addr.s_addr, client_ip,
                      sizeof(client_ip));
            uint16_t client_port = ntohs(c_addr.sin_port);
            SharedChannel s_info = std::make_shared<Channel>(
                cfd, std::make_shared<SubReactor::SocketInfo>(client_port,
                                                              client_ip));
            reactors_[start_index_]->EnrollFd(s_info);
            start_index_ = (start_index_ + 1) % reactor_count_;
            LOG_DEBUG("[client %s:%d]", client_ip, client_port);
          }
          // 产生中断或者读取完fd数据时不处理
          if (errno == EINTR || errno == EAGAIN) {
            continue;
          }
          perror("accept");
          exit(-1);
        } else if (fd == stopfd_ && (fds[i].revents & POLL_IN)) {
          server_run_ = false;
        }
      }
    }
  }
}

void Server::Close() {
  LOG_DEBUG("close server [thread : %lu]", pthread_self());
  WriteEventfd(stopfd_, buf_, "Server::Close");
}

void Server::CloseServer(int invalid_param) { Server::GetInstance()->Close(); }

SharedServer Server::GetInstance() {
  static SharedServer server(new Server());
  return server;
}
}  // namespace TinyWebServer