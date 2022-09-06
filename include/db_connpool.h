#ifndef DB_CONNPOOL_H
#define DB_CONNPOOL_H

#include <mysql/mysql.h>

#include <iostream>

#include "msg_queue.h"

class DBConnPoolInstance;
// 单例数据库连接池
class DBConnPool {
 public:
  static DBConnPool *GetInstance();  // 获取DBConnPool实例
  static void Init(std::string, std::string, std::string, std::string, int,
                   uint32_t);          // 初始化连接池
  DBConnPoolInstance GetConnection();  // 获取连接实例
  bool ReleaseConnection(
      DBConnPoolInstance &db_connpool_instance);  // 释放连接实例
 private:
  DBConnPool();
  ~DBConnPool();

 private:
  static std::string url_;              // 数据库访问地址
  static std::string user_;             // 数据库用户名
  static std::string password_;         // 数据库用户密码
  static std::string db_name_;          // 数据库名
  static int port_;                     // 端口号
  static uint32_t max_conns_;           // 初始化连接数
  MsgQueue<DBConnPoolInstance> conns_;  // 数据库连接池
};

// 数据库连接池实例
class DBConnPoolInstance {
 public:
  DBConnPoolInstance(MYSQL *conn = nullptr);
  DBConnPoolInstance(DBConnPoolInstance &&db_connpool_instance);
  ~DBConnPoolInstance();
  MYSQL *GetConn();

 private:
  MYSQL *conn_;
};

// 数据库连接管理类
class ConnRAII {
 public:
  ConnRAII(DBConnPool *);
  ~ConnRAII();
  MYSQL *GetConn();

 private:
  DBConnPoolInstance conn_;
  DBConnPool *conn_pool_;
};

#endif