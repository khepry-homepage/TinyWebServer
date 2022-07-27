#include "../include/http_conn.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>

int http_conn::m_epollfd = -1; // 全局唯一的epoll实例
int http_conn::m_user_count = -1; // 统计用户的数量


// 设置文件描述符非阻塞
void setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = fcntl(fd, F_SETFL, flags);
  if (ret == -1) {
      perror("fcntl");
      exit(-1);
  }
}

// 添加文件描述符到epoll中
void addfd(int epollfd, int fd, bool one_shot) {
  struct epoll_event ev;
  ev.data.fd = fd;
  // EPOLLRDHUP标识对端socket异常断开事件
  ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  if (one_shot) {
    // 同一个socket只会触发一次读/写事件，
    // 原理在epoll_wait得到就绪fd响应后就会删除对应fd注册的事件，
    // 所以需要重新注册
    ev.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
  setNonBlock(fd);
}

// 删除epoll中的文件描述符
void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
  close(fd);
}

// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下次可读/写
void modfd(int epollfd, int fd, int events) {
  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events = events | EPOLLRDHUP | EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

void http_conn::init(int cfd) {
  m_socketfd = cfd;
  // 设置端口复用
  int reuse = 1;
  setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  addfd(http_conn::m_epollfd, cfd, true);
}

void http_conn::close_conn() {
  if (m_socketfd != -1) {
    removefd(http_conn::m_epollfd, m_socketfd);
    m_socketfd = -1;
    --m_user_count;
  }
}

bool http_conn::read() {
  char buf[1024];
  ssize_t len = recv(m_socketfd, buf, sizeof(buf), 0);
  printf("接受数据:\n%s", buf);
  printf("read success\n");
  return true;
}

bool http_conn::write() {
  printf("write success\n");
  return true;
}

void http_conn::process() {
  read();
  // 解析请求
  printf("parse http request...\n");
  modfd(http_conn::m_epollfd, m_socketfd, EPOLLIN);
  // 生成响应并注册写事件
  printf("produce http response...\n");
  write();
} 