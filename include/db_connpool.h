#ifndef DB_CONNPOOL_H
#define DB_CONNPOOL_H

#include <list>
#include <iostream>
#include <mysql.h>
#include "./locker.h"

// 单例数据库连接池
class DBConnPool {
public:
  static DBConnPool *GetInstance(); // 获取DBConnPool实例
  static bool Init(std::string, std::string, std::string, std::string, int, unsigned int); // 初始化连接池
  MYSQL *GetConnection(); // 获取连接
  bool ReleaseConnection(MYSQL *); // 释放连接
private:
  DBConnPool();
  ~DBConnPool();
private:
  static std::string url; // 数据库访问地址
  static std::string user; // 数据库用户名
  static std::string password; // 数据库用户密码
  static std::string db_name; // 数据库名
  static int port; // 端口号
  static unsigned int max_conns; // 初始化连接数
  std::list<MYSQL*> m_conns; // 数据库连接池
  locker m_lock; // 获取数据库连接时的锁
  sem m_sem; // 判断可用连接数的信号量
};

// 数据库连接管理类
class ConnRAII {
public:
  ConnRAII(MYSQL *, DBConnPool *);
  ~ConnRAII();
  MYSQL *GetConn();
private:
  MYSQL *conn;
  DBConnPool *conn_pool;
};


#endif