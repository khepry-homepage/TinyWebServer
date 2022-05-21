#include "../include/http_parser.h"
using namespace http;
PARSE_STATE HttpParser::parse() {
  PARSE_STATE res_state = CHECK_STATE_REQUESTLINE;
  while (res_state != CHECK_STATE_FINISH || res_state != CHECK_STATE_ERROR) {
    switch (res_state)
    {
    case CHECK_STATE_REQUESTLINE: 
    {
      res_state = parseRequestLine();
      break;
    }
    case CHECK_STATE_HEADER:
    {
      res_state = parseHeader();
      break;
    }
    case CHECK_STATE_BODY: 
    {
      res_state = parseBody();
      break;
    }
    default:
      break;
    }
  }
  return res_state;
}

LINE_STATE HttpParser::parseLine(std::string& line) {
  char* next_line = strstr(text, "\r\n");
  if (next_line == nullptr) {
    return LINE_BAD;
  }
  line.assign(text, next_line - text);
  text = next_line + 2;
  return LINE_OK;
}

PARSE_STATE HttpParser::parseRequestLine() {
  std::string req_line;
  if (parseLine(req_line) == LINE_BAD) {
    return CHECK_STATE_ERROR;
  }
  char* req_line_ptr = const_cast<char*>(req_line.c_str());
  char* tmp_ptr = strstr(req_line_ptr, " ");
  *tmp_ptr++ = '\0';
  if (req_line_ptr == "GET") {
    method = GET;
  }
  else if (req_line_ptr == "POST") {
    method = POST;
  }
  req_line_ptr = strstr(tmp_ptr, "/") + 1;
  tmp_ptr = strstr(req_line_ptr, " ");
  *tmp_ptr++ = '\0';
  uri.assign(req_line_ptr);
  req_line_ptr = strstr(tmp_ptr, "/") + 1;
  if (req_line_ptr == "1.0") {
    version = HTTP_10;
  }
  else if (req_line_ptr == "1.1") {
    version = HTTP_11;
  }
  return CHECK_STATE_HEADER;
}

PARSE_STATE HttpParser::parseHeader() {
  std::string header_line;
  if (parseLine(header_line) == LINE_BAD) {
    return CHECK_STATE_ERROR;
  }
  while (!header_line.empty()) {
    size_t content_begin;
    if ((content_begin = header_line.find(':')) == std::string::npos) {
      return CHECK_STATE_ERROR;
    }
    std::string attribute(header_line.substr(0, content_begin));
    std::string value(header_line.substr(content_begin));
    if (attribute == "Host") header_option.emplace(Host, value);
    else if (attribute == "User-Agent") header_option.emplace(User_Agent, value);
    else if (attribute == "Accept") header_option.emplace(Accept, value);
    else if (attribute == "Referer") header_option.emplace(Referer, value);
    else if (attribute == "Accept-Encoding") header_option.emplace(Accept_Encoding, value);
    else if (attribute == "Accept-Language") header_option.emplace(Accept_Language, value);
    else if (attribute == "Connection") header_option.emplace(Connection, value);

    if (parseLine(header_line) == LINE_BAD) {
      return CHECK_STATE_ERROR;
    }
  }
  return CHECK_STATE_BODY;
}

PARSE_STATE HttpParser::parseBody() {
  if (text == nullptr) {
    return CHECK_STATE_FINISH;
  }
  content = std::string(text);
  return CHECK_STATE_FINISH;
}