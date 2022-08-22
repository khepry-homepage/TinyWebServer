#ifndef LOG_H
#define LOG_H

#include <sys/uio.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <fstream>
#include "./msg_queue.h"

extern const char * default_log_root;

#define LOG_DEBUG(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::DEBUG, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_INFO(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::INFO, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_NOTICE(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::NOTICE, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_WARNING(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::WARNING, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_ERR(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::ERR, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_CRIT(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::CRIT, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_ALERT(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::ALERT, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_EMERG(format, ...) if(Log::GetInstance()->GetLogState()) {Log::GetInstance()->WriteLog(Log::EMERG, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}


#define MAX_LOGNAME	 64
#define MAX_FILENAME 128
#define LOG_BUF_SIZE 256



struct LogMsg {
	char * log_filename;
	char * log_buf;
	LogMsg();
	LogMsg(LogMsg&&);
	~LogMsg();
};

class Log
{
public:
  	enum LOG_LEVEL { EMERG = 0, ALERT, CRIT, ERR, WARNING, NOTICE, INFO, DEBUG }; // 日志级别
public:
	Log();
	~Log();
	bool GetLogState();
	bool WriteLog(Log::LOG_LEVEL, const char *, ...);
	void Flush(); // 将缓冲区数据写入磁盘文件（同步写）
	void HandleAsyncWrite(); // 将缓冲区数据写入磁盘文件（异步写）
	void AppendMsg(LogMsg&&); // 将日志写任务添加到日志写任务阻塞队列中
	static void *Worker(void * args); // 异步线程回调函数
	static void Init(const char *, Log::LOG_LEVEL, int, int, bool); // 日志信息初始化
	static Log *GetInstance();
private:
	static char LOG_FILENAME[MAX_LOGNAME]; // 日志文件名
	static Log::LOG_LEVEL CONSOLE_LOG_LEVEL; // 高于此优先级的日志信息输出到控制台
	static int MAX_LOG_LINE; // 单一日志文件最大行数
	static int MAX_QUEUE_SIZE; // 日志写任务阻塞队列的大小
	static bool ASYNC_WRITE; // 异步写标志
	char m_log_buf[LOG_BUF_SIZE]; // 日志写缓冲区
	int m_write_idx; // 写缓冲区下标
	locker m_mutex; // 日志写动作互斥锁
	MsgQueue<LogMsg> m_queue; // 日志写任务阻塞队列
	pthread_t m_thread; // 异步模式的线程id
	FILE *m_fp; // 日志文件指针
	bool m_log_run;

};





#endif