#include "../include/http_conn.h"

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";


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
  {"default","text/plain"}
};


const char *default_req_uri = "index.html"; // default request uriname
std::string doc_root = "/home/khepry/coding/web_server/static/"; // resource root dirname
int HttpConn::m_epollfd = -1; // 全局唯一的epoll实例
int HttpConn::m_user_count = -1; // 统计用户的数量
DBConnPool *HttpConn::coon_pool = nullptr; // 数据库连接池


/** @fn : int GetGmtTime(char *szGmtTime)
 *  @brief : 获取格林制GMT时间
 *  @param (out) char * szGmtTime : 存放GMT时间的缓存区，外部传入
 *  @return int : szGmtTime的实际长度
 */
int GetGmtTime(char *szGmtTime)
{
    if (szGmtTime == NULL)
    {
        return -1;
    }
    time_t rawTime;
    struct tm *timeInfo;
    char szTemp[30] = {0};
    time(&rawTime);
    timeInfo = gmtime(&rawTime);
    strftime(szTemp,sizeof(szTemp),"%a, %d %b %Y %H:%M:%S GMT", timeInfo);
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


void HttpConn::CloseConn() {
  if (m_socketfd != -1) {
    RemoveFD(m_epollfd, m_socketfd);
    m_socketfd = -1;
    --HttpConn::m_user_count;
  }
}


void HttpConn::Process() {
  // 解析请求
  // printf("parse http request...\n");
  HTTP_CODE read_ret = ProcessRead();
  if (read_ret == NO_REQUEST) {
    // 请求不完整，重新注册读事件
    ModFD(m_epollfd, m_socketfd, EPOLLIN);
    return;
  }
  // 生成响应
  // printf("produce http response...\n");
  bool write_ret = ProcessWrite(read_ret);
  if (m_file_addr) {
    bytes_to_send += m_file_stat.st_size; 
  }
  bytes_to_send += write_idx; 
  if (!write_ret) {
    CloseConn();
  }
  // 注册写事件
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

void HttpConn::Init(int cfd) {
  m_socketfd = cfd;
  // 设置端口复用
  int reuse = 1;
  setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  AddFD(HttpConn::m_epollfd, cfd, true);
}

void HttpConn::Init(HttpRequest *hr, HttpConn *hc) {
  hr->process_state = HttpConn::CHECK_STATE_REQUESTLINE;
  hr->method = HttpRequest::METHOD_NOT_SUPPORT;
  hr->version = HttpRequest::VERSION_NOT_SUPPORT;
  hr->uri = const_cast<char*>(default_req_uri);
  hr->mime_type = nullptr;
  hr->content = nullptr;
  hr->header_option.clear();
  hc->Init();
}


void HttpConn::Init() {
  memset(m_filename, 0, sizeof(m_filename));
  memset(read_buf, 0, sizeof(read_buf));
  memset(write_buf, 0, sizeof(write_buf));
  memset(m_iv, 0, sizeof(struct iovec) * 2);
  read_idx = 0;
  start_idx = 0;
  check_idx = 0;
  write_idx = 0;
  bytes_to_send = 0;
  bytes_have_send = 0;
  m_iv_count = 0;
}

HttpConn::HTTP_CODE
HttpConn::ProcessRead() {
  LINE_STATE line_state = LINE_OK;
  HTTP_CODE parse_state = NO_REQUEST;
  while (hr->process_state == CHECK_STATE_BODY || (line_state = ParseLine()) == LINE_OK) {
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
      else if (parse_state == GET_REQUEST) {
        return DoRequest();
      }
      break;
    }
    case CHECK_STATE_BODY: 
    {
      if ((parse_state = HttpConn::ParseBody(line, hr)) == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      else if (parse_state == GET_REQUEST) {
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

HttpConn::HTTP_CODE
HttpConn::DoRequest() {
  strcpy(m_filename, doc_root.c_str());
  // 若请求uri为空，使用默认uri
  if (strlen(hr->uri) == 0) {
    hr->uri = default_req_uri;
  }
  /* 
   * 处理POST请求
   */
  if (hr->method == HttpRequest::POST) {
    if (strcasecmp(hr->uri, "register") != 0 && strcasecmp(hr->uri, "login") != 0) {
      return FORBIDDEN_REQUEST;
    }
    char username[50]{0};
    char password[50]{0};
    // 提取用户名和密码 username=root&password=123
    int i = 9;
    for (; hr->content[i] != '&'; ++i) {
      username[i - 9] = hr->content[i];
    }
    i += 10;
    for (int j = i; hr->content[j] != '\0'; ++j) {
      password[j - i] = hr->content[j];
    }
    ConnRAII conn_raii(HttpConn::coon_pool);
    MYSQL *mysql = conn_raii.GetConn();
    std::unique_ptr<char> sql_insert(new char[200]); 
    // 注册请求
    if (strcasecmp(hr->uri, "register") == 0) {
      snprintf(sql_insert.get(), 200, "insert into users(username, password) values('%s', '%s')", username, password);
      int res = mysql_query(mysql, sql_insert.get()); // 插入用户名和密码
      if (res == 0) {
        return GET_REQUEST;
      }
      return BAD_REQUEST;
    }
    // 登录请求
    else {
      snprintf(sql_insert.get(), 200, "select * from users where username='%s' and password='%s'", username, password);
      if (mysql_query(mysql, sql_insert.get()) == 0) {
        MYSQL_RES *mysql_res = mysql_store_result(mysql);
        if (mysql_res != nullptr && mysql_num_rows(mysql_res) != 0) {
          return GET_REQUEST;
        }
      }
      return BAD_REQUEST;
    }
  }

  /* 
   *  处理GET请求
   */

  strcpy(m_filename, doc_root.c_str());
  memcpy(m_filename + doc_root.length(), hr->uri, strlen(hr->uri) + 1);
  // 文件是否存在
  if (stat(m_filename, &m_file_stat) == -1) {
    perror("stat");
    return NO_RESOURCE;
  }
  // 判断访问权限
  if (!(m_file_stat.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  // 判断是否是目录
  if ( S_ISDIR( m_file_stat.st_mode ) ) {
      return BAD_REQUEST;
  }
  int fd = open(m_filename, O_RDONLY);
  // 创建虚拟内存映射（磁盘数据拷贝到内存），这样调用read可以直接读内存，而不会发生磁盘→内核→用户内存这两次拷贝
  m_file_addr = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  return FILE_REQUEST;
}

bool HttpConn::ProcessWrite(HTTP_CODE code) {
  switch (code)
  {
  case BAD_REQUEST:
    AddStatusLine(400, error_400_title);
    AddResponseHeader(strlen(error_400_form));
    if (!AddResponseContent(error_400_form)) {
      return false;
    }
    break;
  case NO_RESOURCE:
    AddStatusLine(404, error_404_title);
    AddResponseHeader(strlen(error_404_form));
    if (!AddResponseContent(error_404_form)) {
      return false;
    }
    break;
  case INTERNAL_ERROR:
    AddStatusLine(500, error_500_title);
    AddResponseHeader(strlen(error_500_form));
    if (!AddResponseContent(error_500_form)) {
      return false;
    }
    break;
  case FORBIDDEN_REQUEST:
    AddStatusLine(403, error_403_title);
    AddResponseHeader(strlen(error_403_form));
    if (!AddResponseContent(error_403_form)) {
      return false;
    }
    break;
  case FILE_REQUEST:
    AddStatusLine(200, ok_200_title);
    AddResponseHeader(m_file_stat.st_size);
    m_iv[0].iov_base = write_buf;
    m_iv[0].iov_len = write_idx;
    m_iv[1].iov_base = m_file_addr;
    m_iv[1].iov_len = m_file_stat.st_size;
    m_iv_count = 2;
    return true;
  case GET_REQUEST:
    AddStatusLine(200, ok_200_title);
    AddResponseHeader(0);
    break;
  default:
    return false;;
  }
  m_iv[0].iov_base = write_buf;
  m_iv[0].iov_len = write_idx;
  m_iv_count = 1;
  return true;
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
  // 防止缓冲区溢出攻击
  if (strlen(hr->uri) + 1 > FILENAME_LEN - doc_root.length()) {
    return BAD_REQUEST;
  }
  line = tmp;

  // 若请求uri为空，使用默认uri
  if (strlen(hr->uri) == 0) {
    hr->uri = default_req_uri;
  }
  // 防止缓冲区溢出攻击与设置文件MIME类型
  if (strlen(hr->uri) + 1 > FILENAME_LEN - doc_root.length()) {
    return BAD_REQUEST;
  }
  hr->SetMIME(hr->uri);
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
    bytes_write = writev(m_socketfd, m_iv, m_iv_count);
    if (bytes_write == -1) {
      // 还有更多数据要写
      if (errno == EAGAIN) {
        ModFD(m_epollfd, m_socketfd, EPOLLOUT);
        return true;
      }
      // 解除映射内存
      unmap();
      return false;
    }
    bytes_to_send -= bytes_write;
    bytes_have_send += bytes_write;
    // 响应行和响应头未发送完毕
    if (bytes_have_send <= write_idx) {
      m_iv[0].iov_base = (uint8_t *)m_iv[0].iov_base + bytes_have_send;
      m_iv[0].iov_len -= bytes_have_send;
    }
    // 响应行和响应头已发送完毕
    else {
      m_iv[0].iov_len = 0;
      m_iv[1].iov_base = (uint8_t *)m_iv[1].iov_base + bytes_have_send - write_idx;
      m_iv[1].iov_len -= (bytes_have_send - write_idx);
    }
    if (bytes_to_send == 0) {
      unmap();
      // 不释放连接
      if (strcasecmp(hr->header_option[HttpRequest::Connection], "keep-alive") == 0) {
        ModFD(m_epollfd, m_socketfd, EPOLLIN);
        Init(hr, this);
        return true;
      }
      // 释放连接
      else {
        ModFD(m_epollfd, m_socketfd, EPOLLIN);
        return false;
      }
    }
  }
  return true;
}

bool HttpConn::AddResponseLine(const char *format, ...) {
  if (write_idx >= WRITE_BUFFER_SIZE) {
    return false;
  }
  std::va_list args;
  va_start(args, format);
  int len = vsnprintf(write_buf + write_idx, WRITE_BUFFER_SIZE - write_idx, format, args);
  // 写缓冲区溢出
  if (len >= WRITE_BUFFER_SIZE - write_idx - 1) {
    va_end(args);
    return false;
  }
  write_idx += len;
  va_end(args);
  return true;
}

bool HttpConn::AddStatusLine(int code, const char *content) {
  if (hr->version == HttpRequest::HTTP_10) {
    return AddResponseLine("%s %d %s\r\n", "HTPP/1.0", code, content);
  }
  else if (hr->version == HttpRequest::HTTP_11) {
    return AddResponseLine("%s %d %s\r\n", "HTTP/1.1", code, content);
  }
  return false;
}

bool HttpConn::AddResponseHeader(int content_length) {
  char date[40];
  if (GetGmtTime(date) == -1) {
    perror("GetGmtTime");
    return false;
  }
  if (hr->mime_type != nullptr) {
    AddResponseLine("Content-Type: %s; %s\r\n", hr->mime_type, "charset=utf-8"); 
  }
  if (
    AddResponseLine("Date: %s\r\n", date) &&
    AddResponseLine("Server: %s\r\n", "Apache/2.4.52 (Ubuntu)") &&
    AddResponseLine("Keep-Alive: %s\r\n", "timeout=5") &&
    AddResponseLine("Content-Length: %d\r\n", content_length) &&
    AddResponseLine("Connection: %s\r\n", hr->header_option[HttpRequest::Connection]) &&
    AddResponseLine("Cache-Control: %s\r\n", "public, max-age=0") && 
    AddResponseLine("%s", "\r\n")
  ) {
    return true;
  }
  return false;

}

bool HttpConn::AddResponseContent(const char *content) {
  return AddResponseLine("%s", content);
}

void HttpConn::unmap() {
  if (m_file_addr) {
    munmap(m_file_addr, m_file_stat.st_size);
    m_file_addr = nullptr;
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
    mime_type = mime_map["default"].c_str();
  }
  else {
    mime_type = it->second.c_str();
  }
  return true;
}

int HttpConn::GetSocketfd() {
  return m_socketfd;
}