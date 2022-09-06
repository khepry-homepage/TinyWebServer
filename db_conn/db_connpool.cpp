#include "../include/db_connpool.h"

std::string DBConnPool::url_;
std::string DBConnPool::user_;
std::string DBConnPool::password_;
std::string DBConnPool::db_name_;
int DBConnPool::port_;
uint32_t DBConnPool::max_conns_;

DBConnPool::DBConnPool() : conns_(max_conns_) {
  for (int i = 0; i < max_conns_; ++i) {
    MYSQL *conn = nullptr;
    conn = mysql_init(conn);
    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn) << std::endl;
      exit(1);
    }
    conn = mysql_real_connect(
        conn, DBConnPool::url_.c_str(), DBConnPool::user_.c_str(),
        DBConnPool::password_.c_str(), DBConnPool::db_name_.c_str(),
        DBConnPool::port_, nullptr, 0);

    if (conn == nullptr) {
      std::cout << "Error:" << mysql_error(conn) << std::endl;
      exit(1);
    }
    conns_.Push(DBConnPoolInstance(conn));
  }
}

DBConnPool::~DBConnPool() {}

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

DBConnPoolInstance DBConnPool::GetConnection() { return conns_.Pop(); }

bool DBConnPool::ReleaseConnection(DBConnPoolInstance &db_connpool_instance) {
  if (db_connpool_instance.GetConn() == nullptr) {
    return false;
  }
  conns_.Push(std::move(db_connpool_instance));
  return true;
}

DBConnPoolInstance::DBConnPoolInstance(MYSQL *conn) : conn_(conn) {}
DBConnPoolInstance::DBConnPoolInstance(
    DBConnPoolInstance &&db_connpool_instance) {
  this->conn_ = db_connpool_instance.conn_;
  db_connpool_instance.conn_ = nullptr;
}
DBConnPoolInstance::~DBConnPoolInstance() {
  if (conn_ != nullptr) {
    mysql_close(conn_);
  }
}
MYSQL *DBConnPoolInstance::GetConn() { return conn_; }

ConnRAII::ConnRAII(DBConnPool *conn_pool)
    : conn_pool_(conn_pool), conn_(conn_pool->GetConnection()) {}

ConnRAII::~ConnRAII() { this->conn_pool_->ReleaseConnection(this->conn_); }

MYSQL *ConnRAII::GetConn() { return this->conn_.GetConn(); }