#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <list>

#include "locker.h"

namespace TinyWebServer {
template <typename T>
class MsgQueue : NonCopyable {
 private:
  const int max_queue_size_;
  std::list<T> msg_queue_;  // 消息队列
  mutable locker latch_;    // 辅助消息队列的互斥锁
  sem sem_;                 // 用于判断消息队列资源数的信号量
 public:
  MsgQueue<T>(int size = 0, uint32_t cnt = 0)
      : max_queue_size_(size), sem_(cnt) {}
  ~MsgQueue<T>();
  template <typename _T>
  bool Push(_T&& msg);
  T Pop();
  size_t Size() const;
};

template <typename T>
MsgQueue<T>::~MsgQueue() {
  msg_queue_.clear();
  std::list<T>().swap(msg_queue_);
}

template <typename T>
template <typename _T>
bool MsgQueue<T>::Push(_T&& msg) {
  latch_.lock();
  if (msg_queue_.size() > max_queue_size_) {
    latch_.unlock();
    return false;
  }
  msg_queue_.emplace_back(std::forward<_T>(msg));
  latch_.unlock();
  sem_.post();
  return true;
}

template <typename T>
T MsgQueue<T>::Pop() {
  sem_.wait();
  latch_.lock();
  T msg(std::move(msg_queue_.front()));
  msg_queue_.pop_front();
  latch_.unlock();
  return std::move(msg);
}

template <typename T>
size_t MsgQueue<T>::Size() const {
  latch_.lock();
  size_t size = msg_queue_.size();
  latch_.unlock();
  return size;
}
}  // namespace TinyWebServer
#endif