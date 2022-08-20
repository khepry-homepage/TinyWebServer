# OBJS   代替  依赖文件
# CC     代替  gcc
# CFLAGS 代替  编译命令


ROOT    		= $(PWD)
TEST    		= $(ROOT)/test
HTTP				= $(ROOT)/http
DB_CONN			= $(ROOT)/db_conn
TIMER				= $(ROOT)/timer
CC      		= g++
CFLAGS  		= -lgtest -lgtest_main -pthread -g -L /usr/lib/x86_64-linux-gnu/ -lmysqlclient
RM      		= rm -rf

MAIN_OBJS		= $(ROOT)/main.o $(HTTP)/http_conn.o $(DB_CONN)/db_connpool.o $(ROOT)/server.o $(TIMER)/timer.o
TEST_OBJS		= $(TEST)/test.o $(HTTP)/http_conn.o
MAIN_TARGET	= $(ROOT)/main.cpp $(HTTP)/http_conn.cpp $(DB_CONN)/db_connpool.cpp $(ROOT)/server.cpp $(TIMER)/timer.cpp
TEST_TARGET	= $(TEST)/test.cpp $(HTTP)/http_conn.cpp

server:$(MAIN_OBJS)
	$(CC) $(MAIN_OBJS) -o $(ROOT)/server $(CFLAGS)
	$(RM) $(MAIN_OBJS)
test:$(TEST_OBJS)
	$(CC) $(TEST_TARGET) -o $(TEST)/test  $(CFLAGS)
	$(RM) $(TEST_OBJS)
clean:
	$(RM) $(ROOT)/server $(TEST)/test


