# TinyWebServer
version--1.0

# 数据库"tiny_webserver"的"users"表设计说明

|  字段名  | 长度 | 数据类型 | 是否为空 |          键类型           |               说明               |
| :------: | :--: | :------: | :------: | :-----------------------: | :------------------------------: |
|    id    |  /   |   int    | NOT NULL | UNIQUE KEY AUTO_INCREMENT |              唯一id              |
| username |  8   | varchar  | NOT NULL |        PRIMARY KEY        |  用户名 (字母 + 数字且字母开头)  |
| password |  8   | varchar  | NOT NULL |             /             | 用户密码 (字母 + 数字且字母开头) |

# 创建MYSQL表

```mysql
1. CREATE DATABASE tiny_webserver
2. USE DATABASE tiny_webserver
3. CREATE TABLE users (
        id int NOT NULL UNIQUE KEY AUTO_INCREMENT,
        username varchar(8) NOT NULL PRIMARY KEY,
        password varchar(8) NOT NULL
    )
```

# 编译
`make server -j12`

# 运行
`./server 8080`

# 压力测试
`cd ./webbench-1.5 && webbench -c [concurrency] -t [duration] [host:port]`

# 测试结果
  - 测试环境: 
    - 客户机: Ubuntu20.04 内存8G 2核
    - 服务器: Ubuntu22.04 内存8G 4核
  - 测试结果: 10000+QPS
![image-20220907230526200.png](https://s2.loli.net/2022/09/07/ezJbM4UtBpP9Isn.png)
