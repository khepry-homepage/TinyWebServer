#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#include <unordered_map>

#include "../utils/NonCopyable.h"
#include "http_conn.h"
#include "locker.h"
#include "log.h"
namespace TinyWebServer {
struct Timer {
  Timer(const SharedHttpConn &h_conn);
  ~Timer() = default;

  SharedHttpConn h_conn_;
  time_t expire_time_;
};
typedef std::shared_ptr<Timer> SharedTimer;

extern void AddFD(int epollfd, int fd, bool one_shot);
extern void RemoveFD(int epollfd, int fd);
extern void ModFD(int epollfd, int fd, int events);

class TimerManager : NonCopyable {
 public:
  TimerManager(const int &epollfd);
  ~TimerManager();
  static void Init(time_t max_age, time_t tv_sec);
  int GetTimerfd() const;
  bool IsTimerfd(const int &fd) const;          // 判断是否为定时器fd
  void HandleTick();                            // 超时回调函数
  void AddTimer(const SharedHttpConn &h_conn);  // 给连接添加定时器
  void AdjustTimer(const int &socketfd);  // 调整定时器位置到链表末尾
  std::list<SharedTimer>::iterator DelTimer(
      const int &socketfd);  // 删除连接绑定的定时器，并释放连接

  static time_t max_age_;  // 定时器最大计时时间
  static time_t tv_sec_;   // 定时器定期时间
 private:
  void ReadTimerfd();  // 读取timerfd数据，防止重复触发定时事件

  const int epollfd_;                     // epoll句柄
  std::list<SharedTimer> timer_manager_;  // 定时器管理队列
  int timerfd_;                           // 定时器文件描述符
  std::unordered_map<int, std::list<SharedTimer>::iterator>
      fd_timer_map_;  // 关联fd和定时器的迭代器指针
  locker latch_;      // 操作定时器管理队列时的互斥锁
  uint64_t expirations_number_;
};
}  // namespace TinyWebServer
