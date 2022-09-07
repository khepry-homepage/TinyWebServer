#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <semaphore.h>

#include <exception>

#include "../utils/NonCopyable.h"

namespace TinyWebServer {
class locker : NonCopyable {
 public:
  locker() {
    if (pthread_mutex_init(&latch_, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~locker() { pthread_mutex_destroy(&latch_); }
  bool lock() { return pthread_mutex_lock(&latch_) == 0; }
  bool trylock() { return pthread_mutex_trylock(&latch_) == 0; }
  bool unlock() { return pthread_mutex_unlock(&latch_) == 0; }
  pthread_mutex_t *get() { return &latch_; }

 private:
  pthread_mutex_t latch_;
};

/**
 *  @brief: 存在唤醒丢失问题!
 */
class cond {
 public:
  cond() {
    if (pthread_cond_init(&cond_, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~cond() { pthread_cond_destroy(&cond_); }
  bool wait(pthread_mutex_t *latch) {
    return pthread_cond_wait(&cond_, latch) == 0;
  }
  bool notify() { return pthread_cond_signal(&cond_) == 0; }
  bool notifyAll() { return pthread_cond_broadcast(&cond_) == 0; }

 private:
  pthread_cond_t cond_;
};

class sem {
 public:
  sem(uint32_t value) {
    if (sem_init(&sem_, 0, value) != 0) {
      throw std::exception();
    }
  }
  ~sem() { sem_destroy(&sem_); }
  bool wait() { return sem_wait(&sem_) == 0; }
  bool post() { return sem_post(&sem_) == 0; }
  bool getvalue(int *value) { return sem_getvalue(&sem_, value) == 0; }

 private:
  sem_t sem_;
};
}  // namespace TinyWebServer
#endif