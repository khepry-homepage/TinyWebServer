#ifndef TIMER_H
#define TIMER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctime>
#include <unordered_map>
#include "./locker.h"
#include "./http_conn.h"

class TimerManager;
void ModFD(int epollfd, int fd, int events);

struct s_timer
{
  sockaddr_in c_addr;
  time_t expire_time;
  HttpConn *hc;
  s_timer(HttpConn *, sockaddr_in);
  s_timer(s_timer &&);
  ~s_timer();
};


class TimerManager
{
private:
  std::list<std::unique_ptr<s_timer>> m_manager; // 定时器管理队列
  std::unordered_map<int, std::list<std::unique_ptr<s_timer>>::iterator> m_cache; // 关联fd和定时器的迭代器指针
  locker m_lock; // 操作定时器管理队列时的互斥锁
private: 
  TimerManager() {};
public:
  static int m_epollfd; // 全局epoll实例
  static time_t m_max_age; // 定时器最大计时时间
public:
  static TimerManager *GetInstance();
  static void Init(time_t max_age);
  static void Tick(); // alarm,信号的回调函数
  void HandleTick();
  void AddTimer(HttpConn *, sockaddr_in); // 给连接添加定时器
  void AdjustTimer(const int&); // 调整定时器位置到链表末尾
  std::list<std::unique_ptr<s_timer>>::iterator DelTimer(const int&); // 删除连接绑定的定时器，并释放连接
  ~TimerManager();
};


#endif