#include "../include/http_parser.h"
#include <iostream>
#include <string.h>

HttpParser::HttpParser() {
  hr = new HttpRequest();
  if (hr == nullptr) {
    throw std::exception();
  }
  HttpParser::Init(hr, this);
}

HttpParser::~HttpParser() {
  delete hr;
}

void HttpParser::Init(HttpRequest *hr, HttpParser *hp) {
  hr->process_state = CHECK_STATE_REQUESTLINE;
  hr->method = HttpRequest::METHOD_NOT_SUPPORT;
  hr->version = HttpRequest::VERSION_NOT_SUPPORT;
  hr->uri = const_cast<char*>(default_req_uri);
  hr->content = nullptr;
  hr->header_option.clear();
  hp->Init();
}

void HttpParser::Init() {
  memset(read_buf, 0, sizeof(read_buf));
  read_idx = 0;
  start_idx = 0;
  check_idx = 0;
}

HttpParser::HTTP_CODE
HttpParser::ProcessRead(char *buffer, int &read_idx, int &start_idx, int &check_idx, HttpRequest &hr) {
  LINE_STATE line_state = LINE_OK;
  HTTP_CODE parse_state = NO_REQUEST;
  while (parse_state != GET_REQUEST && (line_state = HttpParser::ParseLine(buffer, read_idx, check_idx)) == LINE_OK) {
    char *line = buffer + start_idx;
    start_idx = check_idx;
    switch (hr.process_state)
    {
    case CHECK_STATE_REQUESTLINE: 
    {
      if ((parse_state = HttpParser::ParseRequestLine(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_HEADER:
    {
      if ((parse_state = HttpParser::ParseHeader(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_BODY: 
    {
      if ((parse_state = HttpParser::ParseBody(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    default:
      break;
    }
  }
  return parse_state;
}

HttpParser::LINE_STATE
HttpParser::ParseLine(char *buffer, const int read_idx, int &check_idx) {
  for (; check_idx < read_idx; ++check_idx) {
    if (buffer[check_idx] == '\r') {
      if (check_idx + 1 == read_idx) {
        return LINE_MORE;
      }
      else if (buffer[check_idx + 1] == '\n') {
        buffer[check_idx++] = '\0';
        buffer[check_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    else if (buffer[check_idx] == '\n') {
      if (check_idx >= 1 && buffer[check_idx - 1] == '\r') {
        buffer[check_idx - 1] = '\0';
        buffer[check_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_MORE;
}

HttpParser::HTTP_CODE
HttpParser::ParseRequestLine(char *line, HttpRequest &hr) {
  if (strstr(line, "GET") != nullptr) {
    hr.method = HttpRequest::GET;
  }
  else if (strstr(line, "POST") != nullptr) {
    hr.method = HttpRequest::POST;
  }
  else {
    return BAD_REQUEST;
  }
  
  char *tmp = nullptr;
  // uri可能是/xxx或http://domain/xxx的形式
  if ((tmp = strstr(line, "http://")) != nullptr) {
    line = tmp;
  }
  if ((line = strstr(line, "/")) == nullptr) {
    return BAD_REQUEST;
  }
  *line++ = '\0';
  // 返回字符为空格或制表符的起始下标
  tmp = strpbrk(line, " \t");
  *tmp++ = '\0';
  hr.uri = line;
  line = tmp;

  if (strcasecmp(line, "HTTP/1.0") == 0) {
    hr.version = HttpRequest::HTTP_10;
  }
  else if (strcasecmp(line, "HTTP/1.1") == 0) {
    hr.version = HttpRequest::HTTP_11;
  }
  else {
    return BAD_REQUEST;
  }
  // 处理状态转换为处理请求头
  hr.process_state = HttpParser::CHECK_STATE_HEADER;
  
  return NO_REQUEST;
}

HttpParser::HTTP_CODE 
HttpParser::ParseHeader(char *line, HttpRequest &hr) {
  if (strcmp(line, "") == 0) {
    if (hr.method == HttpRequest::GET) {
      hr.process_state = CHECK_STATE_FINISH;
      return GET_REQUEST;
    }
    else if (hr.method == HttpRequest::POST) {
      hr.process_state = CHECK_STATE_BODY;
      return NO_REQUEST;
    }
  }
  char *tmp = strstr(line, ":");
  if (tmp == nullptr) {
    return BAD_REQUEST;
  }
  *tmp++ = '\0';
  *tmp++ = '\0';
  if (strcmp(line, "Host") == 0) hr.header_option.emplace(HttpRequest::Host, tmp);
  else if (strcmp(line, "User-Agent") == 0) hr.header_option.emplace(HttpRequest::User_Agent, tmp);
  else if (strcmp(line, "Accept") == 0) hr.header_option.emplace(HttpRequest::Accept, tmp);
  else if (strcmp(line, "Referer") == 0) hr.header_option.emplace(HttpRequest::Referer, tmp);
  else if (strcmp(line, "Accept-Encoding") == 0) hr.header_option.emplace(HttpRequest::Accept_Encoding, tmp);
  else if (strcmp(line, "Accept-Language") == 0) hr.header_option.emplace(HttpRequest::Accept_Language, tmp);
  else if (strcmp(line, "Connection") == 0) hr.header_option.emplace(HttpRequest::Connection, tmp);

  return NO_REQUEST;
}

HttpParser::HTTP_CODE
HttpParser::ParseBody(char *line, HttpRequest &hr) {
  hr.content = line;
  hr.process_state = CHECK_STATE_FINISH;
  return GET_REQUEST;
}