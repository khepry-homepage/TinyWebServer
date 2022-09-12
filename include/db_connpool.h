#ifndef DB_CONNPOOL_H
#define DB_CONNPOOL_H

#include <mysql/mysql.h>

#include <iostream>
#include <memory>

#include "msg_queue.h"

namespace TinyWebServer {
class DBConnPoolInstance;
typedef std::shared_ptr<DBConnPoolInstance> SmartDBConnPoolInstance;
// 单例数据库连接池
class DBConnPool {
 public:
  static DBConnPool *GetInstance();  // 获取DBConnPool实例
  static void Init(std::string url, std::string user, std::string password,
                   std::string db_name, int port,
                   u_int32_t max_conns);    // 初始化连接池
  SmartDBConnPoolInstance GetConnection();  // 获取连接实例
  bool ReleaseConnection(
      SmartDBConnPoolInstance db_connpool_instance);  // 释放连接实例
 private:
  DBConnPool();
  ~DBConnPool() = default;

 private:
  static std::string url_;                   // 数据库访问地址
  static std::string user_;                  // 数据库用户名
  static std::string password_;              // 数据库用户密码
  static std::string db_name_;               // 数据库名
  static int port_;                          // 端口号
  static uint32_t max_conns_;                // 初始化连接数
  MsgQueue<SmartDBConnPoolInstance> conns_;  // 数据库连接池
};

// 数据库连接池实例
class DBConnPoolInstance {
 public:
  DBConnPoolInstance(MYSQL *conn = nullptr);
  ~DBConnPoolInstance();
  MYSQL *GetConn();

 private:
  MYSQL *conn_;
};

// 数据库连接池实例管理类
class ConnInstanceRAII {
 public:
  ConnInstanceRAII(DBConnPool *);
  ~ConnInstanceRAII();
  /**
   * @brief 检查是否成功执行sql语句
   *
   * @param sql
   * @return true
   * @return false
   */
  bool SqlQuery(const char *sql);
  /**
   * @brief 检查sql查询结果是否存在
   *
   * @param sql
   * @return true
   * @return false
   */
  bool SqlQueryIsEmpty(const char *sql);

 private:
  SmartDBConnPoolInstance db_conn_;
  DBConnPool *db_conn_pool_;
};
}  // namespace TinyWebServer

#endif