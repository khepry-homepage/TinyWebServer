#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "db_connpool.h"
#include "http_conn.h"
#include "log.h"
#include "threadpool.h"
#include "timer.h"

class Server {
 private:
  ThreadPool<HttpConn> *thread_pool_;
  TimerManager *timer_manager_;
  Log *log_;

 public:
  void InitThreadPool();     // 初始化线程池
  void InitDBConn();         // 初始化数据库连接池
  void InitTimer();          // 初始化定时器管理器
  void InitLog(bool async);  // 初始化同步/异步日志系统
  void Run(int port);
  Server();
  ~Server();
};

#endif