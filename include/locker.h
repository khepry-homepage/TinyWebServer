#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

class locker {
public:
  locker() {
    if (pthread_mutex_init(&m_mutex, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~locker() {
    pthread_mutex_destroy(&m_mutex);
  }
  bool lock() {
    return pthread_mutex_lock(&m_mutex) == 0;
  }
  bool trylock() {
    return pthread_mutex_trylock(&m_mutex) == 0;
  }
  bool unlock() {
    return pthread_mutex_unlock(&m_mutex) == 0;
  }

private:
  pthread_mutex_t m_mutex;
};



class cond {
public:
  cond() {
    if (pthread_cond_init(&m_cond, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~cond() {
    pthread_cond_destroy(&m_cond);
  }
  bool wait(pthread_mutex_t *mutex) {
    return pthread_cond_wait(&m_cond, mutex) == 0;
  }
  bool notify() {
    return pthread_cond_signal(&m_cond) == 0;
  }
  bool notifyAll() {
    return pthread_cond_broadcast(&m_cond) == 0;
  }
private:
  pthread_cond_t m_cond;
};

class sem {
public:
  sem(unsigned int value) {
    if (sem_init(&m_sem, 0, value) != 0) {
      throw std::exception();
    }
  }
  ~sem() {
    sem_destroy(&m_sem);
  }
  bool wait() {
    return sem_wait(&m_sem) == 0;
  }
  bool post() {
    return sem_post(&m_sem) == 0;
  }
  bool getvalue(int *value) {
    return sem_getvalue(&m_sem, value) == 0;
  }
private: 
  sem_t m_sem;
};

#endif