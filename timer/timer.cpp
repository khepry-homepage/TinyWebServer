#include "../include/timer.h"

s_timer::s_timer(HttpConn *hc_, sockaddr_in addr) : hc(hc_)
{
    c_addr.sin_addr.s_addr = addr.sin_addr.s_addr;
    c_addr.sin_port = addr.sin_port;
    expire_time = time(nullptr) + TimerManager::m_max_age;
}
s_timer::s_timer(s_timer &&st)
{
    this->hc = st.hc;
    this->expire_time = st.expire_time;
    this->c_addr = st.c_addr;
    st.hc = nullptr;
}
s_timer::~s_timer()
{
    if (hc != nullptr)
    {
        char clientIP[16];
        inet_ntop(AF_INET, &c_addr.sin_addr.s_addr, clientIP, sizeof(clientIP));
        printf("释放连接 - client IP is %s and port is %d\n", clientIP, ntohs(c_addr.sin_port));
        hc->CloseConn();
        hc = nullptr;
    }
}

int TimerManager::m_epollfd = -1;
time_t TimerManager::m_max_age = -1;

void TimerManager::Init(time_t max_age)
{
    m_max_age = max_age;
}

TimerManager *TimerManager::GetInstance()
{
    static TimerManager tm;
    return &tm;
}

void TimerManager::Tick(int invalid_param)
{
    TimerManager *tm = TimerManager::GetInstance();
    tm->HandleTick();
}

void TimerManager::HandleTick()
{
    std::list<std::unique_ptr<s_timer>>::iterator it = m_manager.begin();
    time_t curr_time = time(nullptr);
    while (it != m_manager.end() && curr_time >= (*it)->expire_time)
    {
        s_timer *st_ptr = it->get();
        int fd = st_ptr->hc->GetSocketfd(); // 失活时间超过m_max_age，清理本次连接
        it = DelTimer(fd);
    }
}

void TimerManager::AddTimer(HttpConn *hc, sockaddr_in &addr)
{
    std::unique_ptr<s_timer> st_ptr(new s_timer(hc, addr));
    m_lock.lock();
    m_manager.emplace_back(std::move(st_ptr));
    m_cache.emplace(hc->GetSocketfd(), --m_manager.end());
    m_lock.unlock();
}

void TimerManager::AdjustTimer(const int &socketfd)
{
    m_lock.lock();
    std::list<std::unique_ptr<s_timer>>::iterator it;
    try
    {
        it = m_cache.at(socketfd);
    }
    catch (std::out_of_range)
    {
        m_lock.unlock();
        return;
    }
    std::unique_ptr<s_timer> st_ptr(std::move(*it));
    st_ptr->expire_time = time(nullptr) + TimerManager::m_max_age;
    m_manager.erase(it);
    m_manager.emplace_back(std::move(st_ptr));
    m_cache[socketfd] = --m_manager.end();
    m_lock.unlock();
}

std::list<std::unique_ptr<s_timer>>::iterator TimerManager::DelTimer(const int &socketfd)
{
    m_lock.lock();
    std::list<std::unique_ptr<s_timer>>::iterator it;
    try
    {
        it = m_cache.at(socketfd);
    }
    catch (std::out_of_range)
    {
        m_lock.unlock();
        return m_manager.end();
    }
    it = m_manager.erase(it);
    m_cache.erase(socketfd);
    m_lock.unlock();
    return it;
}

TimerManager::~TimerManager()
{
    m_manager.clear();
    m_cache.clear();
}
