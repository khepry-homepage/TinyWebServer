#include "../include/db_connpool.h"

std::string DBConnPool::url;
std::string DBConnPool::user;
std::string DBConnPool::password;
std::string DBConnPool::db_name;
int DBConnPool::port;
unsigned int DBConnPool::max_conns;

DBConnPool::DBConnPool() : m_sem(DBConnPool::max_conns) {
  for (int i = 0; i < max_conns; ++i) {
    MYSQL *conn = nullptr;
    conn = mysql_init(conn);
    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn);
      exit(1);
    }
    conn = mysql_real_connect(conn, DBConnPool::url.c_str(), DBConnPool::user.c_str(),
      DBConnPool::password.c_str(), DBConnPool::db_name.c_str(), DBConnPool::port, nullptr, 0);

    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn);
      exit(1);
    }
    m_conns.push_back(conn);
  }
}

DBConnPool::~DBConnPool() {
  m_lock.lock();
  if (m_conns.size() > 0) {
    std::list<MYSQL *>::iterator it;
    for (it = m_conns.begin(); it != m_conns.end(); ++it) {
      mysql_close(*it);
    }
    m_conns.clear();
  }
  m_lock.unlock();
}

void DBConnPool::Init(std::string url, std::string user, std::string password, std::string db_name, int port, unsigned int max_conns) {
  DBConnPool::url = url;
  DBConnPool::user = user;
  DBConnPool::password = password;
  DBConnPool::db_name = db_name;
  DBConnPool::port = port;
  DBConnPool::max_conns = max_conns;
}

DBConnPool *DBConnPool::GetInstance() {
  static DBConnPool m_conns;
  return &m_conns;
}

MYSQL *DBConnPool::GetConnection() {
  m_sem.wait();
  m_lock.lock();
  MYSQL *conn = m_conns.front();
  m_conns.pop_front();
  m_lock.unlock();
  return conn;
}

bool DBConnPool::ReleaseConnection(MYSQL *conn) {
  if (conn == nullptr) {
    return false;
  }
  m_lock.lock();
  m_conns.push_back(conn);
  m_lock.unlock();
  m_sem.post();
  return true;
}

ConnRAII::ConnRAII(DBConnPool *cp) : conn_pool(cp) {
  this->conn = cp->GetConnection();
}

ConnRAII::~ConnRAII() {
  this->conn_pool->ReleaseConnection(this->conn);
}

MYSQL *ConnRAII::GetConn() {
  return this->conn;
}