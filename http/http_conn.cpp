#include "../include/http_conn.h"

namespace TinyWebServer {
// 定义HTTP响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
    "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form =
    "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form =
    "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form =
    "There was an unusual problem serving the requested file.\n";

// 支持的MIME类型
std::unordered_map<std::string, std::string> mime_map = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".js", "application/javascript"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".svg", "image/svg+xml"},
    {".css", "text/css"},
    {"default", "text/plain"}};

const char *default_req_uri = "index.html";  // default request uriname
std::string doc_root =
    "/home/khepry/coding/web_server/static/";  // resource root dirname
std::atomic<int> HttpConn::user_count_ = 0;    // 统计用户的数量
DBConnPool *HttpConn::coon_pool_ = nullptr;    // 数据库连接池

/** @fn : int GetGmtTime(char *szGmtTime)
 *  @brief : 获取格林制GMT时间
 *  @param (out) char * szGmtTime : 存放GMT时间的缓存区，外部传入
 *  @return int : szGmtTime的实际长度
 */
int GetGmtTime(char *szGmtTime) {
  if (szGmtTime == NULL) {
    return -1;
  }
  time_t rawTime;
  struct tm *timeInfo;
  char szTemp[30] = {0};
  time(&rawTime);
  timeInfo = gmtime(&rawTime);
  strftime(szTemp, sizeof(szTemp), "%a, %d %b %Y %H:%M:%S GMT", timeInfo);
  memcpy(szGmtTime, szTemp, strlen(szTemp) + 1);
  return strlen(szGmtTime);
}

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

HttpConn::HttpConn(const int &epollfd) : epollfd_(epollfd) {
  h_request_ = new HttpRequest();
  if (h_request_ == nullptr) {
    LOG_ERR("%s %s", log_buf_,
            "error: failed in creating HttpRequest instance");
    throw std::exception();
  }
  HttpConn::Init(h_request_, this);
  ++user_count_;
}

HttpConn::~HttpConn() {
  LOG_DEBUG("释放连接 - client IP is %s and port is %d [thread %lu]",
            client_ip_, port_, pthread_self());
  CloseConn();
  delete h_request_;
}

void HttpConn::Init(HttpRequest *h_request, HttpConn *h_conn) {
  h_request->process_state_ = HttpConn::CHECK_STATE_REQUESTLINE;
  h_request->method_ = HttpRequest::METHOD_NOT_SUPPORT;
  h_request->version_ = HttpRequest::VERSION_NOT_SUPPORT;
  h_request->uri_ = const_cast<char *>(default_req_uri);
  h_request->mime_type_ = nullptr;
  h_request->content_ = nullptr;
  h_request->header_option_.clear();
  h_conn->Init();
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char *line,
                                               HttpRequest *h_request) {
  if (strstr(line, "GET") != nullptr) {
    h_request->method_ = HttpRequest::GET;
  } else if (strstr(line, "POST") != nullptr) {
    h_request->method_ = HttpRequest::POST;
  } else {
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
  h_request->uri_ = line;
  // 防止缓冲区溢出攻击
  if (strlen(h_request->uri_) + 1 > FILENAME_LEN - doc_root.length()) {
    return BAD_REQUEST;
  }
  line = tmp;

  // 若请求uri为空，使用默认uri
  if (strlen(h_request->uri_) == 0) {
    h_request->uri_ = default_req_uri;
  }
  // 防止缓冲区溢出攻击与设置文件MIME类型
  if (strlen(h_request->uri_) + 1 > FILENAME_LEN - doc_root.length()) {
    return BAD_REQUEST;
  }
  h_request->SetMIME(h_request->uri_);
  line = tmp;
  if (strcasecmp(line, "HTTP/1.0") == 0) {
    h_request->version_ = HttpRequest::HTTP_10;
  } else if (strcasecmp(line, "HTTP/1.1") == 0) {
    h_request->version_ = HttpRequest::HTTP_11;
  } else {
    return BAD_REQUEST;
  }
  // 处理状态转换为处理请求头
  h_request->process_state_ = HttpConn::CHECK_STATE_HEADER;

  return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeader(char *line, HttpRequest *h_request) {
  if (strcmp(line, "") == 0) {
    if (h_request->method_ == HttpRequest::GET) {
      h_request->process_state_ = CHECK_STATE_FINISH;
      return GET_REQUEST;
    } else if (h_request->method_ == HttpRequest::POST) {
      h_request->process_state_ = CHECK_STATE_BODY;
      return NO_REQUEST;
    }
  }
  char *tmp = strstr(line, ":");
  if (tmp == nullptr) {
    return BAD_REQUEST;
  }
  *tmp++ = '\0';
  *tmp++ = '\0';
  if (strcmp(line, "Host") == 0)
    h_request->header_option_.emplace(HttpRequest::Host, tmp);
  else if (strcmp(line, "User-Agent") == 0)
    h_request->header_option_.emplace(HttpRequest::User_Agent, tmp);
  else if (strcmp(line, "Accept") == 0)
    h_request->header_option_.emplace(HttpRequest::Accept, tmp);
  else if (strcmp(line, "Referer") == 0)
    h_request->header_option_.emplace(HttpRequest::Referer, tmp);
  else if (strcmp(line, "Accept-Encoding") == 0)
    h_request->header_option_.emplace(HttpRequest::Accept_Encoding, tmp);
  else if (strcmp(line, "Accept-Language") == 0)
    h_request->header_option_.emplace(HttpRequest::Accept_Language, tmp);
  else if (strcmp(line, "Connection") == 0)
    h_request->header_option_.emplace(HttpRequest::Connection, tmp);

  return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseBody(char *line, HttpRequest *h_request) {
  h_request->content_ = line;
  h_request->process_state_ = CHECK_STATE_FINISH;
  return GET_REQUEST;
}

void HttpConn::Init() {
  memset(log_buf_, 0, sizeof(log_buf_));
  memset(filename_, 0, sizeof(filename_));
  memset(read_buf_, 0, sizeof(read_buf_));
  memset(write_buf_, 0, sizeof(write_buf_));
  memset(iv_, 0, sizeof(struct iovec) * 2);
  log_buf_idx_ = 0;
  read_idx_ = 0;
  start_idx_ = 0;
  check_idx_ = 0;
  write_idx_ = 0;
  bytes_to_send_ = 0;
  bytes_have_send_ = 0;
  iv_count_ = 0;
}

void HttpConn::Init(int cfd, const char *client_ip, const uint16_t &port) {
  HttpConn::Init(h_request_, this);
  socketfd_ = cfd;
  // 设置端口复用
  int reuse = 1;
  setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  AddFD(epollfd_, cfd, true);
  strcpy(client_ip_, client_ip);
  port_ = port;
}

bool HttpConn::Read() {
  int bytes_read = 0;
  while (true) {
    bytes_read =
        recv(socketfd_, read_buf_ + read_idx_, READ_BUFFER_SIZE - read_idx_, 0);
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
    read_idx_ += bytes_read;
  }
  return true;
}

bool HttpConn::Write() {
  // 所有响应数据已发送完毕
  if (bytes_to_send_ == 0) {
    ModFD(epollfd_, socketfd_, EPOLLIN);
    HttpConn::Init(h_request_, this);
    return true;
  }
  int bytes_write = 0;
  while (true) {
    bytes_write = writev(socketfd_, iv_, iv_count_);
    if (bytes_write == -1) {
      // 还有更多数据要写
      if (errno == EAGAIN) {
        ModFD(epollfd_, socketfd_, EPOLLOUT);
        return true;
      }
      // 解除映射内存
      UnMap();
      return false;
    }
    bytes_to_send_ -= bytes_write;
    bytes_have_send_ += bytes_write;
    // 响应行和响应头未发送完毕
    if (bytes_have_send_ <= write_idx_) {
      iv_[0].iov_base = (uint8_t *)iv_[0].iov_base + bytes_have_send_;
      iv_[0].iov_len -= bytes_have_send_;
    }
    // 响应行和响应头已发送完毕
    else {
      iv_[0].iov_len = 0;
      iv_[1].iov_base =
          (uint8_t *)iv_[1].iov_base + bytes_have_send_ - write_idx_;
      iv_[1].iov_len -= (bytes_have_send_ - write_idx_);
    }
    if (bytes_to_send_ == 0) {
      UnMap();
      // 不释放连接
      if (h_request_->header_option_.find(HttpRequest::Connection) !=
              h_request_->header_option_.end() &&
          strcasecmp(h_request_->header_option_[HttpRequest::Connection],
                     "keep-alive") == 0) {
        ModFD(epollfd_, socketfd_, EPOLLIN);
        HttpConn::Init(h_request_, this);
        return true;
      }
      // 释放连接
      else {
        return false;
      }
    }
  }
  return true;
}

void HttpConn::Process() {
  // 解析请求
  // printf("parse http request...\n");
  HTTP_CODE read_ret = ProcessRead();
  if (read_ret == NO_REQUEST) {
    // 请求不完整，重新注册读事件
    ModFD(epollfd_, socketfd_, EPOLLIN);
    return;
  }
  // 生成响应
  // printf("produce http response...\n");
  bool write_ret = ProcessWrite(read_ret);
  LOG_INFO("%s", log_buf_);
  if (file_addr_) {
    bytes_to_send_ += file_stat_.st_size;
  }
  bytes_to_send_ += write_idx_;
  if (!write_ret) {
    LOG_ERR("%s %s", log_buf_,
            "error: failed in calling ProcessWrite(HTTP_CODE)");
    CloseConn();
  }
  // 注册写事件
  ModFD(epollfd_, socketfd_, EPOLLOUT);
}

HttpConn::HTTP_CODE HttpConn::ProcessRead() {
  LINE_STATE line_state = LINE_OK;
  HTTP_CODE parse_state = NO_REQUEST;
  while (h_request_->process_state_ == CHECK_STATE_BODY ||
         (line_state = ParseLine()) == LINE_OK) {
    char *line = read_buf_ + start_idx_;
    start_idx_ = check_idx_;
    switch (h_request_->process_state_) {
      case CHECK_STATE_REQUESTLINE: {
        log_buf_idx_ +=
            snprintf(log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_,
                     "[client %s] %s ", client_ip_, line);
        if ((parse_state = HttpConn::ParseRequestLine(line, h_request_)) ==
            BAD_REQUEST) {
          return BAD_REQUEST;
        }
        break;
      }
      case CHECK_STATE_HEADER: {
        if ((parse_state = HttpConn::ParseHeader(line, h_request_)) ==
            BAD_REQUEST) {
          return BAD_REQUEST;
        } else if (parse_state == GET_REQUEST) {
          return DoRequest();
        }
        break;
      }
      case CHECK_STATE_BODY: {
        if ((parse_state = HttpConn::ParseBody(line, h_request_)) ==
            BAD_REQUEST) {
          return BAD_REQUEST;
        } else if (parse_state == GET_REQUEST) {
          return DoRequest();
        }
        break;
      }
      default: {
        return INTERNAL_ERROR;
      }
    }
  }
  if (line_state == LINE_BAD) {
    parse_state = BAD_REQUEST;
  }
  return parse_state;
}

HttpConn::HTTP_CODE HttpConn::DoRequest() {
  strcpy(filename_, doc_root.c_str());
  // 若请求uri为空，使用默认uri
  if (strlen(h_request_->uri_) == 0) {
    h_request_->uri_ = default_req_uri;
  }
  /*
   * 处理POST请求
   */
  if (h_request_->method_ == HttpRequest::POST) {
    if (strcasecmp(h_request_->uri_, "register") != 0 &&
        strcasecmp(h_request_->uri_, "login") != 0) {
      return FORBIDDEN_REQUEST;
    }
    char username[50]{0};
    char password[50]{0};
    // 提取用户名和密码 username=root&password=123
    int i = 9;
    for (; h_request_->content_[i] != '&'; ++i) {
      username[i - 9] = h_request_->content_[i];
    }
    i += 10;
    for (int j = i; h_request_->content_[j] != '\0'; ++j) {
      password[j - i] = h_request_->content_[j];
    }
    ConnInstanceRAII conn_ins_raii(HttpConn::coon_pool_);
    std::unique_ptr<char[]> sql_insert(new char[200]);
    // 注册请求
    if (strcasecmp(h_request_->uri_, "register") == 0) {
      snprintf(sql_insert.get(), 200,
               "insert into users(username, password) values('%s', '%s')",
               username, password);
      return conn_ins_raii.SqlQuery(sql_insert.get()) ? GET_REQUEST
                                                      : BAD_REQUEST;
    }
    // 登录请求
    else {
      snprintf(sql_insert.get(), 200,
               "select * from users where username='%s' and password='%s'",
               username, password);
      if (!conn_ins_raii.SqlQueryIsEmpty(sql_insert.get())) {
        return GET_REQUEST;
      }
      return BAD_REQUEST;
    }
  }

  /*
   *  处理GET请求
   */

  strcpy(filename_, doc_root.c_str());
  memcpy(filename_ + doc_root.length(), h_request_->uri_,
         strlen(h_request_->uri_) + 1);
  // 文件是否存在
  if (stat(filename_, &file_stat_) == -1) {
    perror("stat");
    return NO_RESOURCE;
  }
  // 判断访问权限
  if (!(file_stat_.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  // 判断是否是目录
  if (S_ISDIR(file_stat_.st_mode)) {
    return BAD_REQUEST;
  }
  int fd = open(filename_, O_RDONLY);
  // 创建虚拟内存映射（磁盘数据拷贝到内存），这样调用read可以直接读内存，而不会发生磁盘→内核→用户内存这两次拷贝
  file_addr_ =
      (char *)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  return FILE_REQUEST;
}

bool HttpConn::ProcessWrite(HTTP_CODE code) {
  switch (code) {
    case FILE_REQUEST:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 200,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(200, ok_200_title);
      AddResponseHeader(file_stat_.st_size);
      iv_[0].iov_base = write_buf_;
      iv_[0].iov_len = write_idx_;
      iv_[1].iov_base = file_addr_;
      iv_[1].iov_len = file_stat_.st_size;
      iv_count_ = 2;
      return true;
    case GET_REQUEST:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 200,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(200, ok_200_title);
      AddResponseHeader(0);
      break;
    case BAD_REQUEST:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 400,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(400, error_400_title);
      AddResponseHeader(strlen(error_400_form));
      if (!AddResponseContent(error_400_form)) {
        return false;
      }
      break;
    case FORBIDDEN_REQUEST:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 403,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(403, error_403_title);
      AddResponseHeader(strlen(error_403_form));
      if (!AddResponseContent(error_403_form)) {
        return false;
      }
      break;
    case NO_RESOURCE:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 404,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(404, error_404_title);
      AddResponseHeader(strlen(error_404_form));
      if (!AddResponseContent(error_404_form)) {
        return false;
      }
      break;
    case INTERNAL_ERROR:
      log_buf_idx_ += snprintf(
          log_buf_ + log_buf_idx_, LOG_BUF_SIZE - log_buf_idx_, "%d %s", 500,
          h_request_->header_option_[HttpRequest::User_Agent]);
      AddStatusLine(500, error_500_title);
      AddResponseHeader(strlen(error_500_form));
      if (!AddResponseContent(error_500_form)) {
        return false;
      }
      break;
    default:
      return false;
      ;
  }
  iv_[0].iov_base = write_buf_;
  iv_[0].iov_len = write_idx_;
  iv_count_ = 1;
  return true;
}

HttpConn::LINE_STATE HttpConn::ParseLine() {
  for (; check_idx_ < read_idx_; ++check_idx_) {
    if (read_buf_[check_idx_] == '\r') {
      if (check_idx_ + 1 == read_idx_) {
        return LINE_MORE;
      } else if (read_buf_[check_idx_ + 1] == '\n') {
        read_buf_[check_idx_++] = '\0';
        read_buf_[check_idx_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    } else if (read_buf_[check_idx_] == '\n') {
      if (check_idx_ >= 1 && read_buf_[check_idx_ - 1] == '\r') {
        read_buf_[check_idx_ - 1] = '\0';
        read_buf_[check_idx_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_MORE;
}

bool HttpConn::AddResponseLine(const char *format, ...) {
  if (write_idx_ >= WRITE_BUFFER_SIZE) {
    return false;
  }
  std::va_list args;
  va_start(args, format);
  int len = vsnprintf(write_buf_ + write_idx_, WRITE_BUFFER_SIZE - write_idx_,
                      format, args);
  // 写缓冲区溢出
  if (len >= WRITE_BUFFER_SIZE - write_idx_ - 1) {
    va_end(args);
    return false;
  }
  write_idx_ += len;
  va_end(args);
  return true;
}

bool HttpConn::AddStatusLine(int code, const char *content_) {
  if (h_request_->version_ == HttpRequest::HTTP_10) {
    return AddResponseLine("%s %d %s\r\n", "HTPP/1.0", code, content_);
  } else if (h_request_->version_ == HttpRequest::HTTP_11) {
    return AddResponseLine("%s %d %s\r\n", "HTTP/1.1", code, content_);
  }
  return false;
}

bool HttpConn::AddResponseHeader(int content_length) {
  char date[40];
  if (GetGmtTime(date) == -1) {
    perror("GetGmtTime");
    return false;
  }
  if (h_request_->mime_type_ != nullptr) {
    AddResponseLine("Content-Type: %s; %s\r\n", h_request_->mime_type_,
                    "charset=utf-8");
  }
  if (h_request_->header_option_.find(HttpRequest::Connection) !=
      h_request_->header_option_.end()) {
    AddResponseLine("Connection: %s\r\n",
                    h_request_->header_option_[HttpRequest::Connection]);
  } else {
    AddResponseLine("Connection: %s\r\n", "Close");
  }
  if (AddResponseLine("Date: %s\r\n", date) &&
      AddResponseLine("Server: %s\r\n", "Apache/2.4.52 (Ubuntu)") &&
      AddResponseLine("Keep-Alive: %s\r\n", "timeout=5") &&
      AddResponseLine("Content-Length: %d\r\n", content_length) &&
      AddResponseLine("Cache-Control: %s\r\n", "public, max-age=0") &&
      AddResponseLine("%s", "\r\n")) {
    return true;
  }
  return false;
}

bool HttpConn::AddResponseContent(const char *content_) {
  return AddResponseLine("%s", content_);
}

void HttpConn::UnMap() {
  if (file_addr_) {
    munmap(file_addr_, file_stat_.st_size);
    file_addr_ = nullptr;
  }
}

bool HttpRequest::SetMIME(const char *filename) {
  const char *extension = strstr(filename, ".");
  if (extension == nullptr) {
    return false;
  }
  std::unordered_map<std::string, std::string>::iterator it;
  // 设置MIME类型
  if ((it = mime_map.find(std::string(extension))) == mime_map.end()) {
    mime_type_ = mime_map["default"].c_str();
  } else {
    mime_type_ = it->second.c_str();
  }
  return true;
}

int HttpConn::GetSocketfd() { return socketfd_; }

void HttpConn::CloseConn() {
  if (socketfd_ != -1) {
    RemoveFD(epollfd_, socketfd_);
    socketfd_ = -1;
    --user_count_;
  }
}
}  // namespace TinyWebServer