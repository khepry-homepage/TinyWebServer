#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

class http_conn {
public: 
  static int m_epollfd; // 全局唯一的epoll实例
  static int m_user_count; // 统计用户的数量

  http_conn() {};
  ~http_conn() {};
  void init(int); // 初始化接受的客户端连接信息
  void close_conn(); // 关闭连接
  bool read(); // 读取socket
  bool write(); // 写入socket
  void process(); // 处理客户端请求
private:
  int m_socketfd; // 该http连接的socket

};


#endif