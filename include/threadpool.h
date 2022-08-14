#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "locker.h"
#include <list>
#include <sys/sysinfo.h>
#include <ctime>

template <typename T>
class threadpool {
public:
  threadpool(size_t mmr = 1000) 
  : m_threads(nullptr), m_max_requests(mmr), m_queue_tasks(0), m_thread_run(true) {
    // 初始化线程数 = 2 * 核心数 + 1
    m_thread_number = 2 * get_nprocs() + 1;
    m_threads = new pthread_t[m_thread_number];
    if (m_threads == nullptr) {
      throw std::exception();
    }

    // 创建m_thread_number个线程
    for (int i = 0; i < m_thread_number; ++i) {
      printf("create the %dth thread\n", i);
      if (pthread_create(m_threads + i, nullptr, threadpool::worker, this) != 0) {
        throw std::exception();
      }
      pthread_detach(m_threads[i]);
    }
  }
  ~threadpool() {
    delete[] m_threads;
    m_thread_run = false;
  }
  bool append_task(T * task) {
    m_queue_locker.lock();
    // 大于最大连接处理数
    if (m_requests.size() == m_max_requests) {
      m_queue_locker.unlock();
      return false;
    }
    m_requests.push_back(task);
    m_queue_tasks.post();
    m_queue_locker.unlock();
    return true;
  }
  static void * worker(void * args) {
    threadpool *tp = (threadpool *)args;
    tp->run();
    return nullptr;
  }
  void run() {
    while (m_thread_run) {
      m_queue_tasks.wait();
      m_queue_locker.lock();
      T *task = m_requests.front();
      if (task == nullptr) {
        m_queue_locker.unlock();
        throw std::exception();
      }
      m_requests.pop_front();
      m_queue_locker.unlock();
      /* to do */
      task->Process();
    }
  }
private:
  // 创建线程数
  size_t m_thread_number;

  // 线程池数组
  pthread_t * m_threads;

  // 请求队列中最大的请求数
  size_t m_max_requests;

  // 请求队列
  std::list<T *> m_requests;

  // 操作请求队列的互斥锁
  locker m_queue_locker;

  // 信号量用于判断队列中的任务数
  sem m_queue_tasks;

  // 线程池的状态
  bool m_thread_run;
};




#endif