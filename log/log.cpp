#include "../include/log.h"

namespace TinyWebServer {
const char *DEFAULT_ACCESSLOG_ROOT = "./access_log";
const char *DEFAULT_ERRORLOG_ROOT = "./error_log";

LogMsg::LogMsg() {
  log_filename_ = new char[MAX_FILENAME];
  log_buf_ = new char[LOG_BUF_SIZE];
}

LogMsg::~LogMsg() {
  delete[] log_filename_;
  delete[] log_buf_;
}

/** @fn : int getFileLine(const char *filename)
 *  @brief : 获取文件行数
 *  @param (in) const char *filename : 读取的文件所在路径，外部传入
 *  @return int :
 * 					- ≥ 0 : 文件行数
 * 					- < 0 : 文件不存在
 */
int GetFileLine(const char *filename) {
  int nLines = 0;
  std::ifstream input_stream;
  char str[LOG_BUF_SIZE];
  int line = 0;
  input_stream.open(
      filename,
      std::ios::in);  // ios::in 只读方式读取文件，文件不存在不会自动创建
  if (input_stream.fail()) {  // 文件打开失败: 返回-1
    return -1;
  }
  while (input_stream.getline(str, LOG_BUF_SIZE)) {
    ++line;
  }
  input_stream.close();
  return line;
}

char Log::LOG_FILENAME[MAX_LOGNAME];
Log::LOG_LEVEL Log::CONSOLE_LOG_LEVEL = Log::DEBUG;
int Log::MAX_LOG_LINE = 1000;
int Log::MAX_QUEUE_SIZE = 1000;
bool Log::ASYNC_WRITE = true;

Log::Log() : log_queue_(1000), fp_(nullptr), log_run_(true) {
  if (mkdir(DEFAULT_ACCESSLOG_ROOT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) |
      mkdir(DEFAULT_ERRORLOG_ROOT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) !=
          0) {
    if (!(errno & EEXIST)) {
      exit(-1);
    }
  }
  if (ASYNC_WRITE) {
    if (pthread_create(&thread_, nullptr, Log::Worker, this) != 0) {
      throw std::exception();
    }
  }
}

Log::~Log() {
  log_run_ = false;
  log_queue_.Push(nullptr);
  if (ASYNC_WRITE) {
    pthread_join(thread_, nullptr);
  }
}

void Log::Init(const char *log_filename, Log::LOG_LEVEL log_level,
               int max_log_line, int max_queue_size, bool async) {
  strcpy(LOG_FILENAME, log_filename);
  CONSOLE_LOG_LEVEL = log_level;
  MAX_LOG_LINE = max_log_line;
  MAX_QUEUE_SIZE = max_queue_size;
  ASYNC_WRITE = async;
}

Log *Log::GetInstance() {
  static Log log;
  return &log;
}

void *Log::Worker(void *args) {
  Log *log = (Log *)args;
  log->HandleAsyncWrite();
  return nullptr;
}

bool Log::GetLogState() { return log_run_; }

bool Log::WriteLog(Log::LOG_LEVEL log_level, const char *format, ...) {
  time_t timer = time(nullptr);
  tm t;
  localtime_r(&timer, &t);
  char level[16];
  switch (log_level) {
    case EMERG:
      strcpy(level, "[EMERG]:");
      break;
    case ALERT:
      strcpy(level, "[ALERT]:");
      break;
    case CRIT:
      strcpy(level, "[CRIT]:");
      break;
    case ERR:
      strcpy(level, "[ERR]:");
      break;
    case WARNING:
      strcpy(level, "[WARNING]:");
      break;
    case NOTICE:
      strcpy(level, "[NOTICE]:");
      break;
    case INFO:
      strcpy(level, "[INFO]:");
      break;
    case DEBUG:
      strcpy(level, "[DEBUG]:");
      break;
    default:
      break;
  }
  latch_.lock();
  memset(log_buf_, 0, LOG_BUF_SIZE);
  int len = snprintf(log_buf_ + write_idx_, LOG_BUF_SIZE - write_idx_,
                     "%s %d-%02d-%02d %02d:%02d:%02d ", level, t.tm_year + 1900,
                     t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  write_idx_ += len;

  std::va_list args;
  va_start(args, format);
  len = vsnprintf(log_buf_ + write_idx_, LOG_BUF_SIZE - 2 - write_idx_, format,
                  args);
  // 写缓冲区溢出
  if (len >= LOG_BUF_SIZE - 2 - write_idx_) {
    write_idx_ = 0;
    va_end(args);
    latch_.unlock();
    return false;
  }
  write_idx_ += len;
  va_end(args);
  snprintf(log_buf_ + write_idx_, 3, "\r\n");  // 添加换行符
  SharedLogMsg log_msg = std::make_shared<LogMsg>();
  if (log_level >= Log::WARNING) {  // 访问日志
    snprintf(log_msg->log_filename_, MAX_FILENAME,
             "%s/access.%s.%d-%02d-%02d.log", DEFAULT_ACCESSLOG_ROOT,
             Log::LOG_FILENAME, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  } else {  // 错误日志
    snprintf(log_msg->log_filename_, MAX_FILENAME,
             "%s/error.%s.%d-%02d-%02d.log", DEFAULT_ERRORLOG_ROOT,
             Log::LOG_FILENAME, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  }
  int suffix = 1;
  int line = 0;
  while ((line = GetFileLine(log_msg->log_filename_)) >= MAX_LOG_LINE) {
    suffix += line;
    if (log_level >= Log::WARNING) {  // 访问日志
      snprintf(log_msg->log_filename_, MAX_FILENAME,
               "%s/access.%s.%d-%02d-%02d_line_%d.log", DEFAULT_ACCESSLOG_ROOT,
               Log::LOG_FILENAME, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
               suffix);
    } else {  // 错误日志
      snprintf(log_msg->log_filename_, MAX_FILENAME,
               "%s/error.%s.%d-%02d-%02d.log", DEFAULT_ERRORLOG_ROOT,
               Log::LOG_FILENAME, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }
  }
  if (log_level <
      Log::CONSOLE_LOG_LEVEL) {  // 低于控制终端日志输出级别的日志刷盘
    if (Log::ASYNC_WRITE) {  // 异步写
      strcpy(log_msg->log_buf_, log_buf_);
      log_queue_.Push(log_msg);
    } else {  // 同步写
      fp_ = fopen(log_msg->log_filename_, "a+");
      fputs(log_buf_, fp_);
      fclose(fp_);
    }
  } else {  // 高于等于控制终端日志输出级别的日志在控制台输出
    printf("%s", log_buf_);
  }
  write_idx_ = 0;
  latch_.unlock();
  return true;
}

void Log::Flush() { fflush(fp_); }

void Log::HandleAsyncWrite() {
  while (log_run_) {
    SharedLogMsg log_msg = log_queue_.Pop();
    if (log_msg != nullptr) {
      fp_ = fopen(log_msg->log_filename_, "a+");
      fputs(log_msg->log_buf_, fp_);
      fclose(fp_);
    }
  }
}
}  // namespace TinyWebServer