#pragma once

#include <sys/sysinfo.h>

#include <ctime>
#include <list>
#include <memory>

#include "../utils/NonCopyable.h"
#include "log.h"
#include "msg_queue.h"

namespace TinyWebServer {
template <typename T>
class ThreadPool : NonCopyable {
 public:
  static void Init(const size_t &max_requests);
  static ThreadPool<T> *GetInstance();  // 获取线程池实例
  static void *Worker(void *args);
  void Run();
  bool AppendTask(T task);

 private:
  ThreadPool();
  ~ThreadPool();

 private:
  // 请求队列中最大的请求数
  static size_t max_requests_;  // 未显式初始化，默认0

  // 创建线程数
  size_t thread_number_;

  // 线程池数组
  std::unique_ptr<pthread_t[]> threads_;

  // 请求队列
  MsgQueue<T> requests_;

  // 线程池的状态
  bool thread_run_;
};

template <typename T>
size_t ThreadPool<T>::max_requests_ = 1000;

template <typename T>
void ThreadPool<T>::Init(const size_t &max_requests) {
  max_requests_ = max_requests;
}

template <typename T>
ThreadPool<T> *ThreadPool<T>::GetInstance() {  // 获取线程池实例
  static ThreadPool<T> thread_pool;
  return &thread_pool;
}

template <typename T>
void *ThreadPool<T>::Worker(void *args) {
  ThreadPool<T> *thread_pool = (ThreadPool<T> *)args;
  thread_pool->Run();
  return nullptr;
}

template <typename T>
void ThreadPool<T>::Run() {
  while (thread_run_) {
    T task = requests_.Pop();
    if (task != nullptr) {
      /* to do */
      task->Process();
    }
  }
}

template <typename T>
bool ThreadPool<T>::AppendTask(T task) {
  // 大于最大连接处理数
  if (requests_.Size() == max_requests_) {
    return false;
  }
  requests_.Push(task);
  return true;
}

template <typename T>
ThreadPool<T>::ThreadPool()
    : threads_(nullptr), requests_(max_requests_), thread_run_(true) {
  // 初始化线程数 = 2 * 核心数 + 1
  thread_number_ = 2 * get_nprocs() + 1;
  // throw std::bad_alloc when fail
  threads_ = std::make_unique<pthread_t[]>(thread_number_);
  // 创建thread_number_个线程
  for (int i = 0; i < thread_number_; ++i) {
    LOG_DEBUG("create the %dth thread", i);
    if (pthread_create(threads_.get() + i, nullptr, ThreadPool::Worker, this) !=
        0) {
      throw std::exception();
    }
  }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
  thread_run_ = false;
  for (int i = 0; i < thread_number_; ++i) {
    requests_.Push(nullptr);
  }
  for (int i = 0; i < thread_number_; ++i) {
    pthread_join(threads_[i], nullptr);
  }
}
}  // namespace TinyWebServer