#ifndef TINY_HTTP_PARSER_H
#define TINY_HTTP_PARSER_H
#include <string.h>
#include <unordered_map>

namespace http {

  static std::string default_req_uri = "index.html"; // default request uriname
  static std::string doc_root = "/home/khepry/coding/web_server/html/"; // resource root dirname
  struct HttpRequest;

  class HttpParser {
  public:
    enum LINE_STATE { LINE_OK, LINE_BAD, LINE_MORE };
    enum PROCESS_STATE { CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_BODY, CHECK_STATE_FINISH, CHECK_STATE_ERROR, NO_RESOURCE };
    static PROCESS_STATE ProcessRead(char* buffer, int& index, int& line_start, HttpRequest& rq);
    PROCESS_STATE DoRequest();
    static LINE_STATE ParseLine(char* buffer, int& index);
    static PROCESS_STATE ParseRequestLine(char* line, HttpRequest& rq);
    static PROCESS_STATE ParseHeader(char* line, HttpRequest& rq);
    static PROCESS_STATE ParseBody(char* line, HttpRequest& rq);

  };

  struct HttpRequest {
    enum HTTP_VERSION { HTTP_10 = 0, HTTP_11, VERSION_NOT_SUPPORT };
    enum HTTP_METHOD { GET = 0, POST, PUT, METHOD_NOT_SUPPORT };
    enum HTTP_HEADER { Host, Connection, Upgrade_Insecure_Requests, User_Agent, Accept, Referer, Accept_Encoding, Accept_Language };
    HttpRequest() : process_state(HttpParser::CHECK_STATE_REQUESTLINE), version(VERSION_NOT_SUPPORT), method(METHOD_NOT_SUPPORT), uri(default_req_uri) {};
    HttpParser::PROCESS_STATE process_state;
    HTTP_METHOD method;
    std::string uri;
    HTTP_VERSION version;
    std::string content;
    std::unordered_map<HTTP_HEADER, std::string> header_option;
  };

}



#endif