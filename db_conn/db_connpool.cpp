#include "../include/db_connpool.h"

namespace TinyWebServer {
std::string DBConnPool::url_;
std::string DBConnPool::user_;
std::string DBConnPool::password_;
std::string DBConnPool::db_name_;
int DBConnPool::port_;
uint32_t DBConnPool::max_conns_;

DBConnPool::DBConnPool() : conns_(max_conns_) {
  for (int i = 0; i < max_conns_; ++i) {
    MYSQL *conn = mysql_init(nullptr);
    MYSQL *real_conn = mysql_real_connect(
        conn, DBConnPool::url_.c_str(), DBConnPool::user_.c_str(),
        DBConnPool::password_.c_str(), DBConnPool::db_name_.c_str(),
        DBConnPool::port_, nullptr, 0);
    if (real_conn == nullptr) {
      std::cout << "Error:" << mysql_error(real_conn) << std::endl;
      mysql_close(conn);
      mysql_library_end();
      exit(1);
    }
    conns_.Push(std::make_shared<DBConnPoolInstance>(real_conn));
  }
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

SmartDBConnPoolInstance DBConnPool::GetConnection() { return conns_.Pop(); }

bool DBConnPool::ReleaseConnection(
    SmartDBConnPoolInstance db_connpool_instance) {
  if (db_connpool_instance.get()->GetConn() == nullptr) {
    return false;
  }
  conns_.Push(std::move(db_connpool_instance));
  return true;
}

DBConnPoolInstance::DBConnPoolInstance(MYSQL *conn) : conn_(conn) {}
DBConnPoolInstance::~DBConnPoolInstance() {
  if (conn_ != nullptr) {
    mysql_close(conn_);
    mysql_library_end();
  }
}
MYSQL *DBConnPoolInstance::GetConn() { return conn_; }

ConnInstanceRAII::ConnInstanceRAII(DBConnPool *db_conn_pool)
    : db_conn_pool_(db_conn_pool), db_conn_(db_conn_pool->GetConnection()) {}

ConnInstanceRAII::~ConnInstanceRAII() {
  this->db_conn_pool_->ReleaseConnection(this->db_conn_);
}

bool ConnInstanceRAII::SqlQuery(const char *sql) {
  return mysql_query(db_conn_->GetConn(), sql) == 0;
}
bool ConnInstanceRAII::SqlQueryIsEmpty(const char *sql) {
  if (mysql_query(db_conn_->GetConn(), sql) == 0) {
    MYSQL_RES *sql_res = mysql_store_result(db_conn_->GetConn());
    if (sql_res != nullptr && mysql_num_rows(sql_res) != 0) {
      mysql_free_result(sql_res);
      return false;
    }
    mysql_free_result(sql_res);
  }
  return true;
}
}  // namespace TinyWebServer