#include "../include/wrap_socket.h"

tsocket::ClientSocket::~ClientSocket() {
  ::close(c_fd);
}

tsocket::ServerSocket::ServerSocket(int port, const char* ip) : s_port(port), s_ip(ip) {
  bzero(&s_addr, sizeof(s_addr));
  s_addr.sin_family = AF_INET;
  inet_pton(AF_INET, s_ip, &s_addr.sin_addr);
  s_addr.sin_port = htons(s_port);
  s_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (s_fd == -1) {
    std::cout << "create socket error in file: " << __FILE__ << " " << __LINE__ << std::endl;
    exit(0);
  }
}

tsocket::ServerSocket::~ServerSocket() {
  ::close(s_fd);
}


void tsocket::ServerSocket::bind() {
  // ::func_name表示调用全局函数func_name
  int ret = ::bind(s_fd, (struct sockaddr*)&s_addr, sizeof(s_addr));
  if (ret == -1) {
    std::cout << "bind error in file: " << __FILE__ << " " << __LINE__ << std::endl;
    exit(0); 
  }
}

void tsocket::ServerSocket::listen() {
  int ret = ::listen(s_fd, 5);
  if (ret == -1) {
    std::cout << "listen error in file: " << __FILE__ << " " << __LINE__ << std::endl;
    exit(0); 
  }
}

int tsocket::ServerSocket::accept(ClientSocket& c_socket) {
  int connfd = ::accept(s_fd, (struct sockaddr*)&c_socket.c_addr, &c_socket.addr_len);
  if (connfd < 0) {
    std::cout << "accept error in file: " << __FILE__ << " " << __LINE__ << std::endl;
    exit(0); 
  }
  char remote[INET_ADDRSTRLEN];
  std::cout << "connected with ip: "  << inet_ntop(AF_INET, &c_socket.c_addr.sin_addr, remote, INET_ADDRSTRLEN)
            << " port: " << ntohs(c_socket.c_addr.sin_port) << std::endl;
  c_socket.c_fd = connfd;
  return connfd;
}


