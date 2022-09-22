#pragma once
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "../utils/NonCopyable.h"
#include "http_conn.h"
#include "log.h"
#include "reactor.h"
#include "threadpool.h"
#include "timer.h"

namespace TinyWebServer {
class Server;
typedef std::vector<std::unique_ptr<Reactor>> UniqueReactorArr;
typedef std::shared_ptr<Server> SharedServer;
class Server : NonCopyable {
 public:
  ~Server();
  static void CloseServer(int invalid_param);  // 关闭服务器
  static SharedServer GetInstance();
  void InitThreadPool();     // 初始化线程池
  void InitDBConn();         // 初始化数据库连接池
  void InitTimer();          // 初始化定时器管理器
  void InitLog(bool async);  // 初始化同步/异步日志系统
  void Run(int port);
  void Close();

  static bool server_run_;

 private:
  Server(const int &reactor_count = 2);

  std::unique_ptr<pthread_t[]> threads_;
  UniqueReactorArr reactors_;
  Log *log_;
  const int reactor_count_;  // 从reactor数目
  int start_index_;          // Round Robin算法起始索引
  int stopfd_;               // 服务器运行状态fd
  uint64_t buf_;
};
}  // namespace TinyWebServer