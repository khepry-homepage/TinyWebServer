#include "../include/http_parser.h"
#include <iostream>
using namespace http;

HttpParser::PROCESS_STATE
HttpParser::ProcessRead(char* buffer, int& index, int& line_start, HttpRequest& rq) {
  LINE_STATE line_state = LINE_OK;
  
  while (((line_state = HttpParser::ParseLine(buffer, index)) == LINE_OK) && rq.process_state != CHECK_STATE_FINISH && rq.process_state != CHECK_STATE_ERROR) {
    char* line = buffer + line_start;
    line_start = index;
    switch (rq.process_state)
    {
    case CHECK_STATE_REQUESTLINE: 
    {
      rq.process_state = HttpParser::ParseRequestLine(line, rq);
      break;
    }
    case CHECK_STATE_HEADER:
    {
      rq.process_state = HttpParser::ParseHeader(line, rq);
      break;
    }
    case CHECK_STATE_BODY: 
    {
      rq.process_state = HttpParser::ParseBody(line, rq);
      break;
    }
    default:
      break;
    }
  }
  return rq.process_state;
}

HttpParser::LINE_STATE
HttpParser::ParseLine(char* buffer, int& index) {
  char* ptr = strstr((buffer + index), "\r");
  if (ptr == nullptr) {
    return LINE_MORE;
  }
  if (*(ptr + 1) != '\0' && *(ptr + 1) != '\n') {
    return LINE_BAD;
  }
  
  *ptr++ = '\0';
  *ptr++ = '\0';
  index = ptr - buffer;
  return LINE_OK;
}

HttpParser::PROCESS_STATE
HttpParser::ParseRequestLine(char* line, HttpRequest& rq) {
  if (strcmp(line, "GET") == 0) {
    rq.method = HttpRequest::GET;
  }
  else if (strcmp(line, "POST") == 0) {
    rq.method = HttpRequest::POST;
  }
  else {
    return CHECK_STATE_ERROR;
  }
  
  line = strstr(line, "/") + 1;
  char* tmp_ptr = strstr(line, " ");
  *tmp_ptr++ = '\0';
  if (line[0] != '\0') {
    rq.uri.assign(line);
  }
  line = strstr(tmp_ptr, "/") + 1;
  if (strcmp(line, "1.0")) {
    rq.version = HttpRequest::HTTP_10;
  }
  else if (strcmp(line, "1.1")) {
    rq.version = HttpRequest::HTTP_11;
  }
  else {
    return CHECK_STATE_ERROR;
  }
  return CHECK_STATE_HEADER;
}

HttpParser::PROCESS_STATE 
HttpParser::ParseHeader(char* line, HttpRequest& rq) {
  if (*line = '\0') {
    return CHECK_STATE_BODY;
  }
  char* p = strstr(line, ":");
  if (p == nullptr) {
    return CHECK_STATE_ERROR;
  }
  std::string attribute(line, p - line);
  std::string value(p + 1);
  if (attribute == "Host") rq.header_option.emplace(HttpRequest::Host, value);
  else if (attribute == "User-Agent") rq.header_option.emplace(HttpRequest::User_Agent, value);
  else if (attribute == "Accept") rq.header_option.emplace(HttpRequest::Accept, value);
  else if (attribute == "Referer") rq.header_option.emplace(HttpRequest::Referer, value);
  else if (attribute == "Accept-Encoding") rq.header_option.emplace(HttpRequest::Accept_Encoding, value);
  else if (attribute == "Accept-Language") rq.header_option.emplace(HttpRequest::Accept_Language, value);
  else if (attribute == "Connection") rq.header_option.emplace(HttpRequest::Connection, value);

  return CHECK_STATE_HEADER;
}

HttpParser::PROCESS_STATE
HttpParser::ParseBody(char* line, HttpRequest& rq) {
  rq.content = line;
  return CHECK_STATE_FINISH;
}