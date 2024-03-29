#include "./include/server.h"

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("请按照如下格式运行程序: %s port_number\n", basename(argv[0]));
    exit(-1);
  }
  int port = atoi(argv[1]);

  TinyWebServer::SharedServer server = TinyWebServer::Server::GetInstance();

  server->Run(port);

  return 0;
}