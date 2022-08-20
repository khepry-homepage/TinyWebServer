#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <cstring>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>
#include <signal.h>
#include "./threadpool.h"
#include "./http_conn.h"
#include "./db_connpool.h"
#include "./timer.h"




class Server
{
private:
  threadpool<HttpConn> *tp;
public:
  void InitThreadPool();  // 初始化线程池
  void InitDBConn();  // 初始化数据库连接池
  void InitTimer();
  void Run(int);
  Server();
  ~Server();
};



#endif