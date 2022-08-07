# OBJS   代替  依赖文件
# CC     代替  gcc
# CFLAGS 代替  编译命令


ROOT    		= $(PWD)
TEST    		= $(ROOT)/test
HTTP				= $(ROOT)/http
CC      		= g++
CFLAGS  		= -lgtest -lgtest_main -pthread -g
RM      		= rm -rf

MAIN_OBJS		= $(ROOT)/main.o $(HTTP)/http_conn.o
TEST_OBJS		= $(TEST)/test.o $(HTTP)/http_conn.o
MAIN_TARGET	= $(ROOT)/main.cpp $(HTTP)/http_conn.cpp
TEST_TARGET	= $(TEST)/test.cpp $(HTTP)/http_conn.cpp

server:$(MAIN_OBJS)
	$(CC) $(MAIN_OBJS) -o $(ROOT)/server $(CFLAGS)
	$(RM) $(MAIN_OBJS)
test:$(TEST_OBJS)
	$(CC) $(TEST_TARGET) -o $(TEST)/test  $(CFLAGS)
	$(RM) $(TEST_OBJS)
clean:
	$(RM) $(ROOT)/server $(TEST)/test


