#include "../include/timer.h"

namespace TinyWebServer {
s_timer::s_timer(SmartHttpConn h_conn) : h_conn_(h_conn) {
  expire_time_ = time(nullptr) + TimerManager::max_age_;
}
s_timer::s_timer(s_timer &&st) {
  this->h_conn_ = st.h_conn_;
  this->expire_time_ = st.expire_time_;
  st.h_conn_ = st.h_conn_;
}
s_timer::~s_timer() { LOG_DEBUG("清理定时器, 关闭连接..."); }

int TimerManager::epollfd_ = -1;
time_t TimerManager::max_age_ = -1;
time_t TimerManager::tv_sec_ = -1;

TimerManager::TimerManager() {
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

void TimerManager::ReadTimerfd() {
  while (read(timerfd_, &expirations_number_, sizeof(uint64_t)) != -1) {
  }
  if (errno != EAGAIN) {
    perror("read timer");
    exit(-1);
  }
}

TimerManager *TimerManager::GetInstance() {
  static TimerManager timer_manager;
  return &timer_manager;
}

void TimerManager::Init(int epollfd, time_t max_age, time_t tv_sec) {
  epollfd_ = epollfd;
  max_age_ = max_age;
  tv_sec_ = tv_sec;
}

int TimerManager::GetTimerfd() { return timerfd_; }

void TimerManager::HandleTick() {
  ReadTimerfd();
  std::list<std::unique_ptr<s_timer>>::iterator it = timer_manager_.begin();
  time_t curr_time = time(nullptr);
  while (it != timer_manager_.end() && curr_time >= (*it)->expire_time_) {
    s_timer *st_ptr = it->get();
    int fd =
        st_ptr->h_conn_->GetSocketfd();  // 失活时间超过m_max_age，清理本次连接
    it = DelTimer(fd);
  }
  ModFD(epollfd_, timerfd_, EPOLLIN);
}

void TimerManager::AddTimer(SmartHttpConn h_conn) {
  std::unique_ptr<s_timer> st_ptr(new s_timer(h_conn));
  latch_.lock();
  timer_manager_.emplace_back(std::move(st_ptr));
  fd_timer_map_.emplace(h_conn->GetSocketfd(), --timer_manager_.end());
  latch_.unlock();
}

void TimerManager::AdjustTimer(const int &socketfd) {
  std::list<std::unique_ptr<s_timer>>::iterator it;
  latch_.lock();
  try {
    it = fd_timer_map_.at(socketfd);
  } catch (std::out_of_range) {
    latch_.unlock();
    return;
  }
  std::unique_ptr<s_timer> st_ptr(std::move(*it));
  st_ptr->expire_time_ = time(nullptr) + TimerManager::max_age_;
  timer_manager_.erase(it);
  timer_manager_.emplace_back(std::move(st_ptr));
  fd_timer_map_[socketfd] = --timer_manager_.end();
  latch_.unlock();
}

std::list<std::unique_ptr<s_timer>>::iterator TimerManager::DelTimer(
    const int &socketfd) {
  latch_.lock();
  std::list<std::unique_ptr<s_timer>>::iterator it;
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
}  // namespace TinyWebServer