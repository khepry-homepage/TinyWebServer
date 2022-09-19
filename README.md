# TinyWebServer
version--1.0

# 同步IO模拟Proactor并发模型
![同步IO模拟Proactor模式.png](https://s2.loli.net/2022/09/10/4P5EQwzRJqNdO2r.png)

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

# 演示效果
<video src="./demo/演示视频.mp4"></video>

# 压力测试
`cd ./webbench-1.5 && webbench -c [concurrency] -t [duration] [host:port]`

# 测试结果
  - 测试环境: 
    - 客户机: Ubuntu20.04 内存8G 2核
    - 服务器: Ubuntu22.04 内存8G 4核
  - 测试结果: 10000+QPS
![image-20220907230526200.png](https://s2.loli.net/2022/09/07/ezJbM4UtBpP9Isn.png)

# 检查性能瓶颈问题所在
  - `top` 查看当前CPU占用率最高的进程
    ![image-20220911115314977.png](https://s2.loli.net/2022/09/11/Sc3zQlgs7WIi81o.png)
  - `ps -mp [pid] -o THREAD,tid,time` 查看进程内各线程的运行时间
    ![image-20220911120753171.png](https://s2.loli.net/2022/09/11/zxIvuA1LVnNy5oK.png)
  - `gdb attach [pid]`  `thread apply all bt`  查看进程内各线程函数调用栈
    ![image-20220911120931404.png](https://s2.loli.net/2022/09/11/HkE3sD1g4Apbhwr.png)
  通过以上步骤可以发现高并发场景下性能瓶颈主要存在于主线程。

# 存在改进点
使用单线程监听连接并处理IO操作的方式会因为IO操作而阻塞整个线程，使其无法响应新连接，在连接数非常多时存在明显的高并发性能瓶颈，初步设想将IO操作分离到几个单独的IO线程中，主线程只负责接受连接，通过Round Robin算法分发新连接给不同的IO线程。IO线程负责监听注册在其身上的IO事件并处理，而且共享负责处理业务逻辑的线程池，通过消息队列分发计算任务给线程池中的工作线程。
![多Reactor多线程方案.png](https://s2.loli.net/2022/09/11/XqlKZ5Ljh6FSgvC.png)

## To Do

- [ ] 将IO操作的监听和处理分离到单独的IO线程
- [ ] 通过Round Robin算法在IO线程上注册IO事件
