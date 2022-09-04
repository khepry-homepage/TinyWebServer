#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <queue>

#include "locker.h"

template <typename T>
class MsgQueue {
 private:
  const int max_queue_size_;
  std::queue<T> msg_queue_;  // 消息队列
  locker latch_;             // 辅助消息队列的互斥锁
  cond cond_;                // 用于异步读写消息队列的条件锁
 public:
  MsgQueue<T>(int size = 0) : max_queue_size_(size) {}
  ~MsgQueue<T>();
  bool Push(T&& msg);
  T Pop();
  size_t Size();
};

template <typename T>
MsgQueue<T>::~MsgQueue() {
  std::queue<T>().swap(msg_queue_);
}

template <typename T>
bool MsgQueue<T>::Push(T&& msg) {
  latch_.lock();
  if (msg_queue_.size() > max_queue_size_) {
    latch_.unlock();
    return false;
  }
  msg_queue_.push(std::forward<T&&>(msg));
  latch_.unlock();
  cond_.notify();
  return true;
}

template <typename T>
T MsgQueue<T>::Pop() {
  latch_.lock();
  if (msg_queue_.size() <= 0) {
    cond_.wait(latch_.get());
  }
  T msg(std::move(msg_queue_.front()));
  msg_queue_.pop();
  latch_.unlock();
  return std::move(msg);
}

template <typename T>
size_t MsgQueue<T>::Size() {
  latch_.lock();
  size_t size = msg_queue_.size();
  latch_.unlock();
  return size;
}

#endif