// @Author Su Weiyu
// @Created on 2022/5/20 11:00 PM.

#ifndef TINY_SOCKET_H
#define TINY_SOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
namespace tsocket {
  class ClientSocket {
  public: 
    ~ClientSocket();
  public:
    int c_fd;
    socklen_t  addr_len;
    sockaddr_in c_addr;
  };
  
  class ServerSocket {
  public:
    ServerSocket(int port = 8080, const char* ip = nullptr);
    ~ServerSocket();
    void bind();
    void listen();
    int accept(ClientSocket&);
  public: 
    int s_fd;
    int s_port;
    const char* s_ip;
    sockaddr_in s_addr;
  };
}


#endif