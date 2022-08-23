#include "../include/log.h"

const char * default_accesslog_root = "./access_log";
const char * default_errorlog_root = "./error_log";

LogMsg::LogMsg() {
    log_filename = new char[MAX_FILENAME];
    log_buf = new char[LOG_BUF_SIZE];
}
LogMsg::LogMsg(LogMsg&& log_msg) {
    log_filename = log_msg.log_filename;
    log_buf = log_msg.log_buf;
    log_msg.log_filename = nullptr;
    log_msg.log_buf = nullptr;
}	
LogMsg::~LogMsg() {
    delete[] log_filename;
    delete[] log_buf;
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
	input_stream.open(filename, std::ios::in);	// ios::in 只读方式读取文件，文件不存在不会自动创建
	if (input_stream.fail()) {	// 文件打开失败: 返回-1
		return -1;
	}
	while (input_stream.getline(str, LOG_BUF_SIZE))
	{
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

Log::Log() : m_log_run(true), m_fp(nullptr) {
	if (mkdir(default_accesslog_root, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) | 
		mkdir(default_errorlog_root, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		if (!(errno & EEXIST)) {
			exit(-1);
		} 
	}
	if (ASYNC_WRITE) {
		if (pthread_create(&m_thread, nullptr, Log::Worker, this) != 0) {
			throw std::exception();
		}
		pthread_detach(m_thread);
	}
}

Log::~Log() {
    m_log_run = false;
}

bool Log::GetLogState() {
	return m_log_run;
}

bool Log::WriteLog(Log::LOG_LEVEL log_lever, const char *format, ...) {
	time_t _timer = time(nullptr);
	tm *t = localtime(&_timer);
	char level[16];
	switch (log_lever)
	{
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
	m_mutex.lock();
	int len = snprintf(m_log_buf, LOG_BUF_SIZE, 
		"%s %d-%02d-%02d %02d:%02d:%02d ", 
		level, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	m_write_idx += len;
	
	std::va_list args;
	va_start(args, format);
	len = vsnprintf(m_log_buf + m_write_idx, LOG_BUF_SIZE - 2 - m_write_idx, format, args);
	// 写缓冲区溢出
	if (len >= LOG_BUF_SIZE - 2 - m_write_idx) {
		m_write_idx = 0;
		va_end(args);
		m_mutex.unlock();
		return false;
	}
	m_write_idx += len;
	va_end(args);
	snprintf(m_log_buf + m_write_idx, 3, "\r\n"); // 添加换行符 
	LogMsg msg;
	if (log_lever >= Log::WARNING) { // 访问日志
		snprintf(msg.log_filename, MAX_FILENAME, "%s/access.%s.%d-%02d-%02d.log", default_accesslog_root, Log::LOG_FILENAME, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	}
	else { // 错误日志
		snprintf(msg.log_filename, MAX_FILENAME, "%s/error.%s.%d-%02d-%02d.log", default_errorlog_root, Log::LOG_FILENAME, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	}
	
	int suffix = 1;
	int line = 0;
	while ((line = GetFileLine(msg.log_filename)) >= MAX_LOG_LINE) {
		suffix += line;
		if (log_lever >= Log::WARNING) { // 访问日志
			snprintf(msg.log_filename, MAX_FILENAME, "%s/access.%s.%d-%02d-%02d_line_%d.log", default_accesslog_root, Log::LOG_FILENAME, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, suffix);
		}
		else { // 错误日志
			snprintf(msg.log_filename, MAX_FILENAME, "%s/error.%s.%d-%02d-%02d.log", default_errorlog_root, Log::LOG_FILENAME, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
		}
	}
	if (log_lever < Log::CONSOLE_LOG_LEVEL) { // 低于控制终端日志输出级别的日志刷盘
		if (Log::ASYNC_WRITE) { // 异步写
			strcpy(msg.log_buf, m_log_buf);
			m_queue.Push(std::move(msg));
		}
		else { // 同步写
			m_fp = fopen(msg.log_filename, "a+");
			fputs(m_log_buf, m_fp);
			fclose(m_fp);
		}
	}
	else { // 高于等于控制终端日志输出级别的日志在控制台输出
		printf("%s", m_log_buf);
	}
	m_write_idx = 0;
	m_mutex.unlock();
	return true;
}



void Log::Flush() {
	fflush(m_fp);
}

void Log::HandleAsyncWrite() {
	while (m_log_run) {
		LogMsg msg = m_queue.Pop();
		m_fp = fopen(msg.log_filename, "a+");
		fputs(msg.log_buf, m_fp);
		fclose(m_fp);
	}
}


void *Log::Worker(void * args) {
	Log *log = (Log *)args;
	log->HandleAsyncWrite();
	return nullptr;
}

void Log::Init(const char *log_filename, Log::LOG_LEVEL log_level, int max_log_line, int max_queue_size, bool async) {
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
