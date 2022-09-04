#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sys/sysinfo.h>

#include <ctime>
#include <list>

#include "locker.h"
#include "log.h"

template <typename T>
class ThreadPool {
 public:
  ThreadPool(size_t max_requests = 1000)
      : threads_(nullptr),
        max_requests_(max_requests),
        queue_tasks_(0),
        thread_run_(true) {
    // 初始化线程数 = 2 * 核心数 + 1
    thread_number_ = 2 * get_nprocs() + 1;
    threads_ = new pthread_t[thread_number_];
    if (threads_ == nullptr) {
      throw std::exception();
    }

    // 创建thread_number_个线程
    for (int i = 0; i < thread_number_; ++i) {
      printf("create the %dth thread\n", i);
      if (pthread_create(threads_ + i, nullptr, ThreadPool::Worker, this) !=
          0) {
        throw std::exception();
      }
      pthread_detach(threads_[i]);
    }
  }
  ~ThreadPool() {
    delete[] threads_;
    thread_run_ = false;
  }
  bool AppendTask(T *task) {
    queue_latch_.lock();
    // 大于最大连接处理数
    if (requests.size() == max_requests_) {
      queue_latch_.unlock();
      return false;
    }
    requests.push_back(task);
    queue_tasks_.post();
    queue_latch_.unlock();
    return true;
  }
  static void *Worker(void *args) {
    ThreadPool *thread_pool = (ThreadPool *)args;
    thread_pool->Run();
    return nullptr;
  }
  void Run() {
    while (thread_run_) {
      queue_tasks_.wait();
      queue_latch_.lock();
      T *task = requests.front();
      if (task == nullptr) {
        queue_latch_.unlock();
        throw std::exception();
      }
      requests.pop_front();
      queue_latch_.unlock();
      /* to do */
      task->Process();
    }
  }

 private:
  // 创建线程数
  size_t thread_number_;

  // 线程池数组
  pthread_t *threads_;

  // 请求队列中最大的请求数
  size_t max_requests_;

  // 请求队列
  std::list<T *> requests;

  // 操作请求队列的互斥锁
  locker queue_latch_;

  // 信号量用于判断队列中的任务数
  sem queue_tasks_;

  // 线程池的状态
  bool thread_run_;
};

#endif