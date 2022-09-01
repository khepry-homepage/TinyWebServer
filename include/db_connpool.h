#ifndef DB_CONNPOOL_H
#define DB_CONNPOOL_H

#include <mysql/mysql.h>

#include <iostream>
#include <list>

#include "locker.h"

// 单例数据库连接池
class DBConnPool {
 public:
  static DBConnPool *GetInstance();  // 获取DBConnPool实例
  static void Init(std::string, std::string, std::string, std::string, int,
                   unsigned int);   // 初始化连接池
  MYSQL *GetConnection();           // 获取连接
  bool ReleaseConnection(MYSQL *);  // 释放连接
 private:
  DBConnPool();
  ~DBConnPool();

 private:
  static std::string url_;         // 数据库访问地址
  static std::string user_;        // 数据库用户名
  static std::string password_;    // 数据库用户密码
  static std::string db_name_;     // 数据库名
  static int port_;                // 端口号
  static unsigned int max_conns_;  // 初始化连接数
  std::list<MYSQL *> conns_;       // 数据库连接池
  locker lock_;                    // 获取数据库连接时的锁
  sem sem_;                        // 判断可用连接数的信号量
};

// 数据库连接管理类
class ConnRAII {
 public:
  ConnRAII(DBConnPool *);
  ~ConnRAII();
  MYSQL *GetConn();

 private:
  MYSQL *conn_;
  DBConnPool *conn_pool_;
};

#endif