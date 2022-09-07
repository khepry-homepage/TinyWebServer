#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sys/sysinfo.h>

#include <cstdio>
#include <ctime>
#include <list>

#include "log.h"
#include "msg_queue.h"

namespace TinyWebServer {
template <typename T>
class ThreadPool {
 public:
  ThreadPool(size_t max_requests = 1000)
      : threads_(nullptr),
        max_requests_(max_requests),
        requests(max_requests),
        thread_run_(true) {
    // 初始化线程数 = 2 * 核心数 + 1
    thread_number_ = 2 * get_nprocs() + 1;
    // throw std::bad_alloc when fail
    threads_ = std::make_unique<pthread_t[]>(thread_number_);
    // 创建thread_number_个线程
    for (int i = 0; i < thread_number_; ++i) {
      printf("create the %dth thread\n", i);
      if (pthread_create(threads_.get() + i, nullptr, ThreadPool::Worker,
                         this) != 0) {
        throw std::exception();
      }
      pthread_detach(threads_[i]);
    }
  }
  ~ThreadPool() { thread_run_ = false; }
  bool AppendTask(T task) {
    // 大于最大连接处理数
    if (requests.Size() == max_requests_) {
      return false;
    }
    requests.Push(task);
    return true;
  }
  static void *Worker(void *args) {
    ThreadPool *thread_pool = (ThreadPool *)args;
    thread_pool->Run();
    return nullptr;
  }
  void Run() {
    while (thread_run_) {
      T task = requests.Pop();
      /* to do */
      task->Process();
    }
  }

 private:
  // 创建线程数
  size_t thread_number_;

  // 线程池数组
  std::unique_ptr<pthread_t[]> threads_;

  // 请求队列中最大的请求数
  size_t max_requests_;

  // 请求队列
  MsgQueue<T> requests;

  // 线程池的状态
  bool thread_run_;
};
}  // namespace TinyWebServer

#endif