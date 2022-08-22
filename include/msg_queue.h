#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <queue>
#include "./locker.h"


template <typename T>
class MsgQueue
{
private:
    const int m_max_queue_size;
    std::queue<T> m_queue; // 消息队列
    locker m_mutex; // 辅助消息队列的互斥锁
	cond m_cond; // 用于异步读写消息队列的条件锁
public:
    MsgQueue<T>(int size = 0) : m_max_queue_size(size) {}
    ~MsgQueue<T>();
    bool Push(T&&);
    T Pop();
};

template <typename T>
MsgQueue<T>::~MsgQueue() {
    std::queue<T>().swap(m_queue);
}

template <typename T>
bool MsgQueue<T>::Push(T&& msg) {
    m_mutex.lock();
    if (m_queue.size() > m_max_queue_size) {
        m_mutex.unlock();
        return false;
    }
    m_queue.push(std::forward<T&&>(msg));
    m_mutex.unlock();
    m_cond.notify();
    return true;
}

template <typename T>
T MsgQueue<T>::Pop() {
    m_mutex.lock();
    if (m_queue.size() <= 0) {
        m_cond.wait(m_mutex.get());
    }
    T msg(std::move(m_queue.front()));
    m_queue.pop();
    m_mutex.unlock();
    return std::move(msg);
}




#endif