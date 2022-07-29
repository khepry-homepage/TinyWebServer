#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <unordered_map>

const char *default_req_uri = "index.html"; // default request uriname
static std::string doc_root = "/home/khepry/coding/web_server/html/"; // resource root dirname

class HttpParser {
public:
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
  HttpParser();
  ~HttpParser();
  static HTTP_CODE ProcessRead(char *, int &, int &, int &, struct HttpRequest &);
  HTTP_CODE DoRequest();
  static HTTP_CODE ParseRequestLine(char *, struct HttpRequest &);
  static HTTP_CODE ParseHeader(char *, struct HttpRequest &);
  static HTTP_CODE ParseBody(char *, struct HttpRequest &);
  static LINE_STATE ParseLine(char *, const int, int &);
  static void Init(HttpRequest *, HttpParser *); // 初始化http请求解析状态
  void Init();
  const static int READ_BUFFER_SIZE = 2048; // 读缓冲区大小
private:
  struct HttpRequest *hr;
  char read_buf[READ_BUFFER_SIZE];
  int read_idx; // 当前读取的字节位置
  int start_idx; // 当前解析的行起始位置
  int check_idx; // 当前解析的字节位置
};

struct HttpRequest {
  enum HTTP_VERSION { HTTP_10 = 0, HTTP_11, VERSION_NOT_SUPPORT };
  enum HTTP_METHOD { GET = 0, POST, PUT, METHOD_NOT_SUPPORT };
  enum HTTP_HEADER { Host, Connection, Upgrade_Insecure_Requests, User_Agent, Accept, Referer, Accept_Encoding, Accept_Language };
  HttpRequest() : 
    process_state(HttpParser::CHECK_STATE_REQUESTLINE), version(VERSION_NOT_SUPPORT),
    method(METHOD_NOT_SUPPORT), uri(const_cast<char*>(default_req_uri)), content(nullptr) {};
  HttpParser::PROCESS_STATE process_state;
  HTTP_METHOD method;
  HTTP_VERSION version;
  char *uri;
  char *content;
  std::unordered_map<HTTP_HEADER, char *> header_option;
};




#endif