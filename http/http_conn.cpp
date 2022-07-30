#include "../include/http_conn.h"


const char *default_req_uri = "index.html"; // default request uriname
int HttpConn::m_epollfd = -1; // 全局唯一的epoll实例
int HttpConn::m_user_count = -1; // 统计用户的数量


// 设置文件描述符非阻塞
void SetNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = fcntl(fd, F_SETFL, flags);
  if (ret == -1) {
      perror("fcntl");
      exit(-1);
  }
}

// 添加文件描述符到epoll中
void AddFD(int epollfd, int fd, bool one_shot) {
  struct epoll_event ev;
  ev.data.fd = fd;
  // EPOLLRDHUP标识对端socket异常断开事件
  ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  if (one_shot) {
    // 同一个socket只会触发一次读/写事件，
    // 原理在epoll_wait得到就绪fd响应后就会删除对应fd注册的事件，
    // 所以需要重新注册
    ev.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
  SetNonBlock(fd);
}

// 删除epoll中的文件描述符
void RemoveFD(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
  close(fd);
}

// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下次可读/写
void ModFD(int epollfd, int fd, int events) {
  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events = events | EPOLLRDHUP | EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

void HttpConn::Init(int cfd) {
  m_socketfd = cfd;
  // 设置端口复用
  int reuse = 1;
  setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  AddFD(HttpConn::m_epollfd, cfd, true);
}

void HttpConn::CloseConn() {
  if (m_socketfd != -1) {
    RemoveFD(HttpConn::m_epollfd, m_socketfd);
    m_socketfd = -1;
    --m_user_count;
  }
}


void HttpConn::Process() {
  // 解析请求
  printf("parse http request...\n");
  HTTP_CODE read_ret = ProcessRead();
  if (read_ret == NO_REQUEST) {
    ModFD(m_epollfd, m_socketfd, EPOLLIN);
    return;
  }
  // 生成响应
  printf("produce http response...\n");
  bool write_ret = ProcessWrite(read_ret);
  if (!write_ret) {
    CloseConn();
  }
  ModFD(HttpConn::m_epollfd, m_socketfd, EPOLLOUT);
} 

HttpConn::HttpConn() {
  hr = new HttpRequest();
  if (hr == nullptr) {
    throw std::exception();
  }
  HttpConn::Init(hr, this);
}

HttpConn::~HttpConn() {
  delete hr;
}

void HttpConn::Init(HttpRequest *hr, HttpConn *hc) {
  hr->process_state = HttpConn::CHECK_STATE_REQUESTLINE;
  hr->method = HttpRequest::METHOD_NOT_SUPPORT;
  hr->version = HttpRequest::VERSION_NOT_SUPPORT;
  hr->uri = const_cast<char*>(default_req_uri);
  hr->content = nullptr;
  hr->header_option.clear();
  hc->Init();
}


void HttpConn::Init() {
  memset(read_buf, 0, sizeof(read_buf));
  memset(write_buf, 0, sizeof(write_buf));
  read_idx = 0;
  start_idx = 0;
  check_idx = 0;
  write_idx = 0;
  bytes_to_send = 0;
}

HttpConn::HTTP_CODE
HttpConn::ProcessRead() {
  LINE_STATE line_state = LINE_OK;
  HTTP_CODE parse_state = NO_REQUEST;
  while (parse_state != GET_REQUEST && (line_state = ParseLine()) == LINE_OK) {
    char *line = read_buf + start_idx;
    start_idx = check_idx;
    switch (hr->process_state)
    {
    case CHECK_STATE_REQUESTLINE: 
    {
      if ((parse_state = HttpConn::ParseRequestLine(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_HEADER:
    {
      if ((parse_state = HttpConn::ParseHeader(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_BODY: 
    {
      if ((parse_state = HttpConn::ParseBody(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    default:
      break;
    }
  }
  if (line_state == LINE_BAD) {
    parse_state = BAD_REQUEST;
  }
  return parse_state;
}

HttpConn::HTTP_CODE 
HttpConn::ProcessWrite(HttpConn::HTTP_CODE code) {
  return NO_REQUEST;
}

HttpConn::LINE_STATE
HttpConn::ParseLine() {
  for (; check_idx < read_idx; ++check_idx) {
    if (read_buf[check_idx] == '\r') {
      if (check_idx + 1 == read_idx) {
        return LINE_MORE;
      }
      else if (read_buf[check_idx + 1] == '\n') {
        read_buf[check_idx++] = '\0';
        read_buf[check_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    else if (read_buf[check_idx] == '\n') {
      if (check_idx >= 1 && read_buf[check_idx - 1] == '\r') {
        read_buf[check_idx - 1] = '\0';
        read_buf[check_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_MORE;
}

HttpConn::HTTP_CODE
HttpConn::ParseRequestLine(char *line, HttpRequest *hr) {
  if (strstr(line, "GET") != nullptr) {
    hr->method = HttpRequest::GET;
  }
  else if (strstr(line, "POST") != nullptr) {
    hr->method = HttpRequest::POST;
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
  hr->uri = line;
  line = tmp;

  if (strcasecmp(line, "HTTP/1.0") == 0) {
    hr->version = HttpRequest::HTTP_10;
  }
  else if (strcasecmp(line, "HTTP/1.1") == 0) {
    hr->version = HttpRequest::HTTP_11;
  }
  else {
    return BAD_REQUEST;
  }
  // 处理状态转换为处理请求头
  hr->process_state = HttpConn::CHECK_STATE_HEADER;
  
  return NO_REQUEST;
}


HttpConn::HTTP_CODE 
HttpConn::ParseHeader(char *line, HttpRequest *hr) {
  if (strcmp(line, "") == 0) {
    if (hr->method == HttpRequest::GET) {
      hr->process_state = CHECK_STATE_FINISH;
      return GET_REQUEST;
    }
    else if (hr->method == HttpRequest::POST) {
      hr->process_state = CHECK_STATE_BODY;
      return NO_REQUEST;
    }
  }
  char *tmp = strstr(line, ":");
  if (tmp == nullptr) {
    return BAD_REQUEST;
  }
  *tmp++ = '\0';
  *tmp++ = '\0';
  if (strcmp(line, "Host") == 0) hr->header_option.emplace(HttpRequest::Host, tmp);
  else if (strcmp(line, "User-Agent") == 0) hr->header_option.emplace(HttpRequest::User_Agent, tmp);
  else if (strcmp(line, "Accept") == 0) hr->header_option.emplace(HttpRequest::Accept, tmp);
  else if (strcmp(line, "Referer") == 0) hr->header_option.emplace(HttpRequest::Referer, tmp);
  else if (strcmp(line, "Accept-Encoding") == 0) hr->header_option.emplace(HttpRequest::Accept_Encoding, tmp);
  else if (strcmp(line, "Accept-Language") == 0) hr->header_option.emplace(HttpRequest::Accept_Language, tmp);
  else if (strcmp(line, "Connection") == 0) hr->header_option.emplace(HttpRequest::Connection, tmp);

  return NO_REQUEST;
}

HttpConn::HTTP_CODE
HttpConn::ParseBody(char *line, HttpRequest *hr) {
  hr->content = line;
  hr->process_state = CHECK_STATE_FINISH;
  return GET_REQUEST;
}

bool HttpConn::Read() {
  int bytes_read = 0;
  while (true) {
    bytes_read = recv(m_socketfd, read_buf, READ_BUFFER_SIZE - read_idx, 0);
    if (bytes_read == -1) {
      // 已读完所有数据
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      return false;
    }
    // 对方关闭连接
    else if (bytes_read == 0) {
      return false;
    }
    read_idx += bytes_read;
  }
  return true;
}

bool HttpConn::Write() {
  // 所有响应数据已发送完毕
  if (bytes_to_send == 0) {
    ModFD(m_epollfd, m_socketfd, EPOLLIN);
    Init(hr, this);
    return true;
  }
  int bytes_write = 0;
  while (true) {
    bytes_write = send(m_socketfd, write_buf + write_idx, bytes_to_send, 0);
    if (bytes_write == -1) {
      // 还有更多数据要写
      if (errno == EAGAIN) {
        ModFD(m_epollfd, m_socketfd, EPOLLOUT);
        break;
      }
      return false;
    }
    bytes_to_send -= bytes_write;
    write_idx += bytes_write;
    if (bytes_to_send == 0) {

      if (strcasecmp(hr->header_option[HttpRequest::Connection], "keep-alive") == 0) {
        ModFD(m_epollfd, m_socketfd, EPOLLIN);
        Init(hr, this);
        return true;
      }
      else {
        ModFD(m_epollfd, m_socketfd, EPOLLIN);
        return false;
      }
    }
  }
  return true;
}