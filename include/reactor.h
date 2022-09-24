#pragma once
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <functional>

#include "../utils/NonCopyable.h"
#include "threadpool.h"
#include "timer.h"
namespace TinyWebServer {

template <typename... Args>
bool WriteEventfd(const int &fd, uint64_t &buf, const char *format,
                  Args... args);

struct Channel {
  typedef std::shared_ptr<void> SharedVoid;
  Channel(const int &fd, SharedVoid data) : fd_(fd), data_(data) {}
  int fd_;
  SharedVoid data_;
};
typedef std::shared_ptr<Channel> SharedChannel;

class Reactor : NonCopyable {
 public:
  Reactor();
  virtual ~Reactor() = default;
  virtual void Init();
  static void *Worker(void *args) {
    Reactor *reactor = static_cast<Reactor *>(args);
    reactor->EventLoop();
    return nullptr;
  }
  void EventLoop();
  bool EnrollFd(SharedChannel shared_channel);
  bool NotifyDestroy();

 protected:
  virtual void HandleEnrollEvent(const int &fd,
                                 const epoll_event &epoll_event) = 0;
  virtual void HandleCommonEvent(const int &fd,
                                 const epoll_event &epoll_event) = 0;

  bool IsEnrollEvent(const int &fd);
  bool IsStopEvent(const int &fd);
  int ChannelQueueSize();
  void RegisterEnrollEventCallBack(
      const std::function<void(const int &, const epoll_event &)> &func);
  void RegisterCommonEventCallBack(
      const std::function<void(const int &, const epoll_event &)> &func);

  int epollfd_;                            // epoll句柄
  int max_events_;                         // epoll可监听最大事件数
  std::unique_ptr<epoll_event[]> events_;  // epoll数组

  int enrollfd_;  // 注册通知事件fd
  int stopfd_;    // 停止reactor事件循环fd
  uint64_t buf_;  // 用于通知reactor事件到来的8Bytes
  MsgQueue<SharedChannel> channel_queue_;  // 等待注册的连接队列
 private:
  bool run_state_;  // reactor运行状态
  std::function<void(const int &, const epoll_event &)>
      handle_enroll_event_;  // 处理注册事件的回调函数
  std::function<void(const int &, const epoll_event &)>
      handle_common_event_;  // 处理其它事件的回调函数
};

class SubReactor : public Reactor {
 public:
  struct SocketInfo {
    SocketInfo(uint16_t port, const char *client_ip) : port_(port) {
      strcpy(client_ip_, client_ip);
    }
    uint16_t port_ = -1;
    char client_ip_[16];
  };
  SubReactor();
  ~SubReactor() = default;
  void HandleEnrollEvent(const int &fd, const epoll_event &epoll_event);
  void HandleCommonEvent(const int &fd, const epoll_event &epoll_event);

 private:
  std::unordered_map<int, WeakHttpConn> http_conns_;
  std::unique_ptr<TimerManager> timer_manager_;
  ThreadPool<SharedHttpConn> *thread_pool_;
  Log *log_;
};
}  // namespace TinyWebServer
