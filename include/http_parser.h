#ifndef TINY_HTTP_PARSER_H
#define TINY_HTTP_PARSER_H
#include <string.h>
#include <unordered_map>

namespace http {
  enum HTTP_VERSION { HTTP_10 = 0, HTTP_11, VERSION_NOT_SUPPORT };
  enum HTTP_METHOD { GET = 0, POST, PUT, METHOD_NOT_SUPPORT };
  enum HTTP_HEADER { Host, Connection, Upgrade_Insecure_Requests, User_Agent, Accept, Referer, Accept_Encoding, Accept_Language };
  enum LINE_STATE { LINE_OK, LINE_BAD };
  enum PARSE_STATE { CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_BODY, CHECK_STATE_FINISH, CHECK_STATE_ERROR };

  class HttpParser {
  public:
    HttpParser(char* buffer = nullptr) : text(buffer) {};
    ~HttpParser() {};
    PARSE_STATE parse();
    LINE_STATE parseLine(std::string&);
    PARSE_STATE parseRequestLine();
    PARSE_STATE parseHeader();
    PARSE_STATE parseBody();

  private:
    HTTP_VERSION version;
    HTTP_METHOD method;
    std::string uri;
    std::unordered_map<HTTP_HEADER, std::string> header_option;
    std::string content;
    char* text;
  };
}



#endif