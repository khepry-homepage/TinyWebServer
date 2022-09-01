#include "../include/db_connpool.h"

std::string DBConnPool::url_;
std::string DBConnPool::user_;
std::string DBConnPool::password_;
std::string DBConnPool::db_name_;
int DBConnPool::port_;
unsigned int DBConnPool::max_conns_;

DBConnPool::DBConnPool() : sem_(DBConnPool::max_conns_) {
  for (int i = 0; i < max_conns_; ++i) {
    MYSQL *conn = nullptr;
    conn = mysql_init(conn);
    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn);
      exit(1);
    }
    conn = mysql_real_connect(
        conn, DBConnPool::url_.c_str(), DBConnPool::user_.c_str(),
        DBConnPool::password_.c_str(), DBConnPool::db_name_.c_str(),
        DBConnPool::port_, nullptr, 0);

    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn);
      exit(1);
    }
    conns_.push_back(conn);
  }
}

DBConnPool::~DBConnPool() {
  lock_.lock();
  if (conns_.size() > 0) {
    std::list<MYSQL *>::iterator it;
    for (it = conns_.begin(); it != conns_.end(); ++it) {
      mysql_close(*it);
    }
    conns_.clear();
  }
  lock_.unlock();
}

void DBConnPool::Init(std::string url, std::string user, std::string password,
                      std::string db_name, int port, u_int32_t max_conns) {
  DBConnPool::url_ = url;
  DBConnPool::user_ = user;
  DBConnPool::password_ = password;
  DBConnPool::db_name_ = db_name;
  DBConnPool::port_ = port;
  DBConnPool::max_conns_ = max_conns;
}

DBConnPool *DBConnPool::GetInstance() {
  static DBConnPool conns_;
  return &conns_;
}

MYSQL *DBConnPool::GetConnection() {
  sem_.wait();
  lock_.lock();
  MYSQL *conn = conns_.front();
  conns_.pop_front();
  lock_.unlock();
  return conn;
}

bool DBConnPool::ReleaseConnection(MYSQL *conn) {
  if (conn == nullptr) {
    return false;
  }
  lock_.lock();
  conns_.push_back(conn);
  lock_.unlock();
  sem_.post();
  return true;
}

ConnRAII::ConnRAII(DBConnPool *conn_pool) : conn_pool_(conn_pool) {
  this->conn_ = conn_pool_->GetConnection();
}

ConnRAII::~ConnRAII() { this->conn_pool_->ReleaseConnection(this->conn_); }

MYSQL *ConnRAII::GetConn() { return this->conn_; }