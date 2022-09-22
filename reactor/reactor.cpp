#include "../include/reactor.h"

namespace TinyWebServer {

template <typename... Args>
bool WriteEventfd(const int &fd, uint64_t &buf, const char *format,
                  Args... args) {
  if (write(fd, &buf, sizeof(buf)) != 8) {
    LOG_ERR(format, args...);
    return false;
  }
  return true;
}

Reactor::Reactor() { Init(); }

void Reactor::Init() {
  epollfd_ = epoll_create(4);
  enrollfd_ = eventfd(0, EFD_NONBLOCK);
  stopfd_ = eventfd(0, EFD_NONBLOCK);
  if (epollfd_ == -1 || enrollfd_ == -1 || stopfd_ == -1) {
    LOG_ERR("Reactor::Reactor()");
    exit(-1);
  }
  max_events_ = 1024;
  events_ = std::make_unique<epoll_event[]>(1024);
  buf_ = 1;
  run_state_ = true;
  AddFD(epollfd_, enrollfd_, false);
  AddFD(epollfd_, stopfd_, false);
}

void Reactor::EventLoop() {
  while (run_state_) {
    int fd_cnt = epoll_wait(epollfd_, events_.get(), max_events_, -1);
    for (int i = 0; i < fd_cnt; ++i) {
      int fd = events_[i].data.fd;
      if (IsStopEvent(fd)) {
        run_state_ = false;
        break;
      } else if (IsEnrollEvent(fd)) {
        handle_enroll_event_(fd, events_[i]);
      } else {
        handle_common_event_(fd, events_[i]);
      }
    }
  }
  LOG_DEBUG("destroy reactor [thread : %lu]", pthread_self());
}

bool Reactor::EnrollFd(SharedChannel shared_channel) {
  channel_queue_.Push(shared_channel);
  return WriteEventfd(enrollfd_, buf_, "Reactor::EnrollFd()");
}

bool Reactor::NotifyDestroy() {
  return WriteEventfd(stopfd_, buf_, "Reactor::NotifyDestroy()");
}

bool Reactor::IsEnrollEvent(const int &fd) { return fd == enrollfd_; }

bool Reactor::IsStopEvent(const int &fd) { return fd == stopfd_; }

bool Reactor::IsChannelEmpty() { return channel_queue_.Size() == 0; }

void Reactor::RegisterEnrollEventCallBack(
    const std::function<void(const int &, const epoll_event &)> &func) {
  handle_enroll_event_ = func;
}

void Reactor::RegisterCommonEventCallBack(
    const std::function<void(const int &, const epoll_event &)> &func) {
  handle_common_event_ = func;
}

SubReactor::SubReactor() : timer_manager_(nullptr) {
  RegisterEnrollEventCallBack(std::bind(&SubReactor::HandleEnrollEvent, this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
  RegisterCommonEventCallBack(std::bind(&SubReactor::HandleCommonEvent, this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
  thread_pool_ = ThreadPool<SmartHttpConn>::GetInstance();
  timer_manager_ = std::make_unique<TimerManager>(epollfd_);
  log_ = Log::GetInstance();
}

void SubReactor::HandleEnrollEvent(const int &fd,
                                   const epoll_event &epoll_event) {
  uint64_t buf;
  while (read(fd, &buf, sizeof(buf)) != -1) {
  }
  if (errno != EINTR && errno != EAGAIN) {
    LOG_ERR("fail in SubReactor::HandleEnrollEvent");
    exit(-1);
  }
  // 产生中断或者读取完fd数据时不处理
  while (!IsChannelEmpty()) {
    SharedChannel shared_channel = channel_queue_.Pop();
    // 记录客户端连接信息
    SmartHttpConn http_conn(std::make_shared<HttpConn>(epollfd_));
    SocketInfo *socket_info =
        reinterpret_cast<SocketInfo *>(shared_channel->data_.get());
    int cfd = shared_channel->fd_;
    http_conn->Init(cfd, socket_info->client_ip_, socket_info->port_);
    timer_manager_->AddTimer(http_conn);
    if (http_conns_.find(cfd) != http_conns_.end()) {
      http_conns_[cfd] = http_conn;
    } else {
      http_conns_.insert({cfd, http_conn});
    }
    LOG_DEBUG("enroll socketfd in thread : %lu", pthread_self());
  }
}

void SubReactor::HandleCommonEvent(const int &fd,
                                   const epoll_event &epoll_event) {
  if (timer_manager_->IsTimerfd(fd)) {
    timer_manager_->HandleTick();
  } else if (epoll_event.events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
    // 异常事件，清理定时器并关闭连接
    timer_manager_->DelTimer(fd);
    http_conns_.erase(fd);
  } else if (epoll_event.events & EPOLLIN) {
    SmartHttpConn http_conn = http_conns_[fd].lock();
    if (http_conn == nullptr) {
      http_conns_.erase(fd);
      return;
    }
    // 一次性读取socket数据成功
    if (http_conn->Read()) {
      timer_manager_->AdjustTimer(fd);
      thread_pool_->AppendTask(http_conn);
    } else {
      timer_manager_->DelTimer(fd);
      http_conns_.erase(fd);
    }
  } else if (epoll_event.events & EPOLLOUT) {
    SmartHttpConn http_conn = http_conns_[fd].lock();
    if (http_conn == nullptr) {
      http_conns_.erase(fd);
      return;
    }
    // 一次性写入socket数据失败，清理定时器
    if (!http_conn->Write()) {
      timer_manager_->DelTimer(fd);
      http_conns_.erase(fd);
    }
  }
}
}  // namespace TinyWebServer