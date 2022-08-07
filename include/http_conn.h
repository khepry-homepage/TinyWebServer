#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <string.h>
#include <time.h>
#include <cstdarg>

extern const char *default_req_uri;
extern std::string doc_root;

struct HttpRequest;

class HttpConn {
public:
  const static int READ_BUFFER_SIZE = 2048; // 读缓冲区大小
  const static int WRITE_BUFFER_SIZE = 2048; // 写缓冲区大小
  const static int FILENAME_LEN = 128; // 访问资源的文件名大小
  static int m_epollfd; // 全局唯一的epoll实例
  static int m_user_count; // 统计用户的数量

  enum LINE_STATE { LINE_OK, LINE_BAD, LINE_MORE };
  enum PROCESS_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_BODY, CHECK_STATE_FINISH};
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
  enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };  
  HttpConn();
  ~HttpConn();
  void Init(int); // 初始化接受的客户端连接信息
  void CloseConn(); // 关闭连接
  bool Read(); // 读取socket
  bool Write(); // 写入socket
  void Process(); // 处理客户端请求
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
  bool ProcessWrite(HTTP_CODE);
  HTTP_CODE DoRequest();
  static HTTP_CODE ParseRequestLine(char *, HttpRequest *);
  static HTTP_CODE ParseHeader(char *, HttpRequest *);
  static HTTP_CODE ParseBody(char *, HttpRequest *);
  LINE_STATE ParseLine();
  static void Init(HttpRequest *, HttpConn *); // 初始化http请求解析状态
  void Init();
  bool AddStatusLine(int, const char*); // 往写缓冲区添加状态行信息
  bool AddResponseHeader(int); // 往写缓冲区添加响应头信息
  bool AddResponseLine(const char *, ...); // 将格式化的数据写入写缓冲区中
  bool AddResponseContent(const char *); // 往写缓冲区添加错误提示信息
  void unmap();  // 解除映射内存
  // DEBUG_TEST
  void CopyReadBuf(char *buf, int buf_len) {
    memcpy(read_buf + read_idx, buf, buf_len);
    read_idx += buf_len;
  }
  HttpRequest *GetHR() {
    return hr;
  }
private:
  int m_socketfd; // 该http连接的socket
  struct HttpRequest *hr;
  char read_buf[READ_BUFFER_SIZE];
  int read_idx; // 当前读取的字节位置
  int start_idx; // 当前解析的行起始位置
  int check_idx; // 当前解析的字节位置

  char write_buf[WRITE_BUFFER_SIZE];
  int write_idx = 0;
  int bytes_to_send = 0; // 需要发送的字节总数
  int bytes_have_send = 0; // 已经发送的字节总数

  char m_filename[FILENAME_LEN]; // 目标文件的绝对路径
  struct stat m_file_stat;  // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
  char *m_file_addr; // mmap映射的虚拟内存区域地址
  struct iovec m_iv[2]; // I/O向量结构体数组
  int m_iv_count; // I/O向量结构体数组大小
};

struct HttpRequest {
  enum HTTP_VERSION { HTTP_10 = 0, HTTP_11, VERSION_NOT_SUPPORT };
  enum HTTP_METHOD { GET = 0, POST, PUT, METHOD_NOT_SUPPORT };
  enum HTTP_HEADER { Host, Connection, Upgrade_Insecure_Requests, User_Agent, Accept, Referer, Accept_Encoding, Accept_Language };
  HttpRequest() : 
    process_state(HttpConn::CHECK_STATE_REQUESTLINE), version(VERSION_NOT_SUPPORT),
    method(METHOD_NOT_SUPPORT), uri(const_cast<char*>(default_req_uri)), content(nullptr) {};
  HttpConn::PROCESS_STATE process_state;
  HTTP_METHOD method;
  HTTP_VERSION version;
  const char *uri;
  const char *content;
  std::unordered_map<HTTP_HEADER, char *> header_option;
};

#endif