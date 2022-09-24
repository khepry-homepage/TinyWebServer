#pragma once
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h> /* See NOTES */
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "db_connpool.h"
#include "log.h"

namespace TinyWebServer {
extern const char *default_req_uri;
extern std::string doc_root;
struct HttpRequest;
class HttpConn;
typedef std::shared_ptr<HttpConn> SharedHttpConn;
typedef std::weak_ptr<HttpConn> WeakHttpConn;

class HttpConn {
 public:
  enum LINE_STATE { LINE_OK, LINE_BAD, LINE_MORE };
  enum PROCESS_STATE {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_BODY,
    CHECK_STATE_FINISH
  };
  /*
      服务器处理HTTP请求的可能结果，报文解析的结果
      NO_REQUEST          :   请求不完整，需要继续读取客户数据
      GET_REQUEST         :   表示获得了一个完成的客户请求
      BAD_REQUEST         :   表示客户请求语法错误
      NO_RESOURCE         :   表示服务器没有资源
      FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
      FILE_REQUEST        :   文件请求,获取文件成功
      INTERNAL_ERROR      :   表示服务器内部错误
      CLOSED_CONNECTION   :   表示客户端已经关闭连接了
  */
  enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };
  HttpConn(const int &epollfd);
  ~HttpConn();

  static void Init(HttpRequest *h_request,
                   HttpConn *h_coon);  // 初始化http请求解析状态
  static HTTP_CODE ParseRequestLine(char *line, HttpRequest *h_request);
  static HTTP_CODE ParseHeader(char *line, HttpRequest *h_request);
  static HTTP_CODE ParseBody(char *line, HttpRequest *h_request);
  void Init();
  void Init(int cfd, const char *client_ip,
            const uint16_t &port);  // 初始化接受的客户端连接信息
  bool Read();                      // 读取socket
  bool Write();                     // 写入socket
  void Process();                   // 处理客户端请求
  /*
      服务器处理HTTP请求的可能结果，报文解析的结果
      NO_REQUEST          :   请求不完整，需要继续读取客户数据
      GET_REQUEST         :   表示获得了一个完成的客户请求
      BAD_REQUEST         :   表示客户请求语法错误
      NO_RESOURCE         :   表示服务器没有资源
      FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
      FILE_REQUEST        :   文件请求,获取文件成功
      INTERNAL_ERROR      :   表示服务器内部错误
      CLOSED_CONNECTION   :   表示客户端已经关闭连接了
  */
  HTTP_CODE ProcessRead();
  HTTP_CODE DoRequest();
  bool ProcessWrite(HTTP_CODE code);
  LINE_STATE ParseLine();
  bool AddResponseLine(const char *format,
                       ...);  // 将格式化的数据写入写缓冲区中
  bool AddStatusLine(int code,
                     const char *content);  // 往写缓冲区添加状态行信息
  bool AddResponseHeader(int content_len);  // 往写缓冲区添加响应头信息
  bool AddResponseContent(const char *content);  // 往写缓冲区添加错误提示信息
  void UnMap();                                  // 解除映射内存
  int GetSocketfd();  // 获取连接的文件描述符

  static std::atomic<int> user_count_;  // 统计用户的数量
  static DBConnPool *coon_pool_;        // 数据库连接池
 private:
  void CloseConn();  // 关闭连接

  const static int READ_BUFFER_SIZE = 2048;   // 读缓冲区大小
  const static int WRITE_BUFFER_SIZE = 2048;  // 写缓冲区大小
  const static int FILENAME_LEN = 128;        // 访问资源的文件名大小

  const int epollfd_;  // epoll句柄
  int socketfd_;       // 该http连接的socket
  int read_idx_;       // 当前读取的字节位置
  int start_idx_;      // 当前解析的行起始位置
  int check_idx_;      // 当前解析的字节位置
  char read_buf_[READ_BUFFER_SIZE];
  HttpRequest *h_request_;
  char write_buf_[WRITE_BUFFER_SIZE];
  int write_idx_ = 0;
  int bytes_to_send_ = 0;    // 需要发送的字节总数
  int bytes_have_send_ = 0;  // 已经发送的字节总数

  char filename_[FILENAME_LEN];  // 目标文件的绝对路径
  struct stat
      file_stat_;  // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
  char *file_addr_;     // mmap映射的虚拟内存区域地址
  struct iovec iv_[2];  // I/O向量结构体数组
  int iv_count_;        // I/O向量结构体数组大小

  char client_ip_[64];          // 客户端IP信息
  uint16_t port_;               // 客户端端口信息
  char log_buf_[LOG_BUF_SIZE];  // 日志缓冲区
  int log_buf_idx_;
};

struct HttpRequest {
  enum HTTP_VERSION { HTTP_10 = 0, HTTP_11, VERSION_NOT_SUPPORT };
  enum HTTP_METHOD { GET = 0, POST, PUT, METHOD_NOT_SUPPORT };
  enum HTTP_HEADER {
    Host,
    Connection,
    Upgrade_Insecure_Requests,
    User_Agent,
    Accept,
    Referer,
    Accept_Encoding,
    Accept_Language
  };
  HttpRequest()
      : process_state_(HttpConn::CHECK_STATE_REQUESTLINE),
        version_(VERSION_NOT_SUPPORT),
        method_(METHOD_NOT_SUPPORT),
        uri_(const_cast<char *>(default_req_uri)),
        content_(nullptr),
        mime_type_(nullptr){};
  bool SetMIME(const char *filename);  // 获取文件的MIME类型
  HttpConn::PROCESS_STATE process_state_;
  HTTP_METHOD method_;
  HTTP_VERSION version_;
  const char *uri_;
  const char *content_;
  const char *mime_type_;
  std::unordered_map<HTTP_HEADER, const char *> header_option_;
};
}  // namespace TinyWebServer