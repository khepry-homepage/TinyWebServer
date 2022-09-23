#include "../include/timer.h"

namespace TinyWebServer {
Timer::Timer(SmartHttpConn h_conn) : h_conn_(h_conn) {
  expire_time_ = time(nullptr) + TimerManager::max_age_;
}

Timer::~Timer() { LOG_DEBUG("清理定时器, 关闭连接..."); }

time_t TimerManager::max_age_ = -1;
time_t TimerManager::tv_sec_ = -1;

TimerManager::TimerManager(const int &epollfd) : epollfd_(epollfd) {
  timerfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  itimerspec new_value;
  new_value.it_value.tv_sec = tv_sec_;
  new_value.it_value.tv_nsec = 0;
  new_value.it_interval.tv_sec = tv_sec_;
  new_value.it_interval.tv_nsec = 0;
  timerfd_settime(timerfd_, 0, &new_value, nullptr);
  AddFD(epollfd_, timerfd_, true);
}

TimerManager::~TimerManager() {
  RemoveFD(epollfd_, timerfd_);
  timer_manager_.clear();
  fd_timer_map_.clear();
}

void TimerManager::Init(time_t max_age, time_t tv_sec) {
  max_age_ = max_age;
  tv_sec_ = tv_sec;
}
int TimerManager::GetTimerfd() const { return timerfd_; }
bool TimerManager::IsTimerfd(const int &fd) const { return fd == timerfd_; }

void TimerManager::HandleTick() {
  ReadTimerfd();
  std::list<SharedTimer>::iterator it = timer_manager_.begin();
  time_t curr_time = time(nullptr);
  while (it != timer_manager_.end() && curr_time >= (*it)->expire_time_) {
    Timer *st_ptr = it->get();
    int fd =
        st_ptr->h_conn_->GetSocketfd();  // 失活时间超过m_max_age，清理本次连接
    it = DelTimer(fd);
  }
  ModFD(epollfd_, timerfd_, EPOLLIN);
}

void TimerManager::AddTimer(SmartHttpConn h_conn) {
  SharedTimer st_ptr = std::make_shared<Timer>(h_conn);
  latch_.lock();
  timer_manager_.emplace_back(st_ptr);
  fd_timer_map_.emplace(h_conn->GetSocketfd(), --timer_manager_.end());
  latch_.unlock();
}

void TimerManager::AdjustTimer(const int &socketfd) {
  std::list<SharedTimer>::iterator it;
  latch_.lock();
  try {
    it = fd_timer_map_.at(socketfd);
  } catch (std::out_of_range) {
    latch_.unlock();
    return;
  }
  SharedTimer st_ptr(*it);
  st_ptr->expire_time_ = time(nullptr) + TimerManager::max_age_;
  timer_manager_.erase(it);
  timer_manager_.emplace_back(st_ptr);
  fd_timer_map_[socketfd] = --timer_manager_.end();
  latch_.unlock();
}

std::list<SharedTimer>::iterator TimerManager::DelTimer(const int &socketfd) {
  latch_.lock();
  std::list<SharedTimer>::iterator it;
  try {
    it = fd_timer_map_.at(socketfd);
  } catch (std::out_of_range) {
    latch_.unlock();
    return timer_manager_.end();
  }
  it = timer_manager_.erase(it);
  fd_timer_map_.erase(socketfd);
  latch_.unlock();
  return it;
}

void TimerManager::ReadTimerfd() {
  while (read(timerfd_, &expirations_number_, sizeof(uint64_t)) != -1) {
  }
  if (errno != EAGAIN) {
    perror("read timer");
    exit(-1);
  }
}
}  // namespace TinyWebServer