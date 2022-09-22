#ifndef LOG_H
#define LOG_H

#include <errno.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>

#include "msg_queue.h"
namespace TinyWebServer {
#define LOG_DEBUG(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                           \
    Log::GetInstance()->WriteLog(Log::DEBUG, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                     \
  }
#define LOG_INFO(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                          \
    Log::GetInstance()->WriteLog(Log::INFO, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                    \
  }
#define LOG_NOTICE(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                            \
    Log::GetInstance()->WriteLog(Log::NOTICE, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                      \
  }
#define LOG_WARNING(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                             \
    Log::GetInstance()->WriteLog(Log::WARNING, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                       \
  }
#define LOG_ERR(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                         \
    Log::GetInstance()->WriteLog(Log::ERR, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                   \
  }
#define LOG_CRIT(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                          \
    Log::GetInstance()->WriteLog(Log::CRIT, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                    \
  }
#define LOG_ALERT(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                           \
    Log::GetInstance()->WriteLog(Log::ALERT, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                     \
  }
#define LOG_EMERG(format, ...)                                       \
  if (Log::GetInstance()->GetLogState()) {                           \
    Log::GetInstance()->WriteLog(Log::EMERG, format, ##__VA_ARGS__); \
    Log::GetInstance()->Flush();                                     \
  }

#define MAX_LOGNAME 64
#define MAX_FILENAME 128
#define LOG_BUF_SIZE 256

struct LogMsg {
  LogMsg();
  ~LogMsg();
  char *log_filename_;
  char *log_buf_;
};
typedef std::shared_ptr<LogMsg> SharedLogMsg;

class Log {
 public:
  enum LOG_LEVEL {
    EMERG = 0,
    ALERT,
    CRIT,
    ERR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG
  };  // 日志级别

  Log();
  ~Log();

  static void Init(const char *log_filename, Log::LOG_LEVEL log_level,
                   int max_log_line, int max_queue_size,
                   bool async);  // 日志信息初始化
  static Log *GetInstance();
  static void *Worker(void *args);  // 异步线程回调函数
  bool GetLogState();
  bool WriteLog(Log::LOG_LEVEL log_lever, const char *format, ...);
  void Flush();  // 将缓冲区数据写入磁盘文件（同步写）
  void HandleAsyncWrite();  // 将缓冲区数据写入磁盘文件（异步写）

 private:
  static char LOG_FILENAME[MAX_LOGNAME];  // 日志文件名
  static Log::LOG_LEVEL
      CONSOLE_LOG_LEVEL;  // 高于此优先级的日志信息输出到控制台
  static int MAX_LOG_LINE;            // 单一日志文件最大行数
  static int MAX_QUEUE_SIZE;          // 日志写任务阻塞队列的大小
  static bool ASYNC_WRITE;            // 异步写标志
  char log_buf_[LOG_BUF_SIZE];        // 日志写缓冲区
  int write_idx_;                     // 写缓冲区下标
  locker latch_;                      // 日志写动作互斥锁
  MsgQueue<SharedLogMsg> log_queue_;  // 日志写任务阻塞队列
  pthread_t thread_;                  // 异步模式的线程id
  FILE *fp_;                          // 日志文件指针
  bool log_run_;
};
}  // namespace TinyWebServer

#endif