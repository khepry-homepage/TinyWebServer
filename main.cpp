#include "./include/server.h"

// 添加信号捕捉
void addsig(int sig, void(*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); // 将所有信号加入信号集，表示系统支持的信号均为有效信号
    sigaction(sig, &sa, nullptr); // 添加信号捕捉以及信号处理函数
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        printf("请按照如下格式运行程序: %s port_number\n", basename(argv[0]));
        exit(-1);
    }
    int port = atoi(argv[1]);
    addsig(SIGPIPE, SIG_IGN); // 忽略
    Server server;
    
    server.InitThreadPool();

    server.InitDBConn();

    server.Run(port);
    
    return 0;
}