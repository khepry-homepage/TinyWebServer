#ifndef TIMER_H
#define TIMER_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#include <unordered_map>

#include "http_conn.h"
#include "locker.h"
#include "log.h"
namespace TinyWebServer {
class TimerManager;
extern void AddFD(int epollfd, int fd, bool one_shot);
extern void RemoveFD(int epollfd, int fd);
extern void ModFD(int epollfd, int fd, int events);

struct s_timer {
  time_t expire_time_;
  SmartHttpConn h_conn_;
  s_timer(SmartHttpConn h_conn);
  s_timer(s_timer &&st);
  ~s_timer();
};

class TimerManager {
 private:
  std::list<std::unique_ptr<s_timer>> timer_manager_;  // 定时器管理队列
  int timerfd_;  // 定时器文件描述符
  std::unordered_map<int, std::list<std::unique_ptr<s_timer>>::iterator>
      fd_timer_map_;  // 关联fd和定时器的迭代器指针
  locker latch_;      // 操作定时器管理队列时的互斥锁
  uint64_t expirations_number_;

 private:
  TimerManager();
  void ReadTimerfd();  // 读取timerfd数据，防止重复触发定时事件
 public:
  static int epollfd_;     // 全局epoll实例
  static time_t max_age_;  // 定时器最大计时时间
  static time_t tv_sec_;   // 定时器定期时间
 public:
  ~TimerManager();
  static TimerManager *GetInstance();
  static void Init(int epollfd, time_t max_age, time_t tv_sec);
  int GetTimerfd();                       // 获取定时器文件描述符
  void HandleTick();                      // 超时回调函数
  void AddTimer(SmartHttpConn h_conn);    // 给连接添加定时器
  void AdjustTimer(const int &socketfd);  // 调整定时器位置到链表末尾
  std::list<std::unique_ptr<s_timer>>::iterator DelTimer(
      const int &socketfd);  // 删除连接绑定的定时器，并释放连接
};
}  // namespace TinyWebServer

#endif