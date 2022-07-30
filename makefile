# OBJS   代替  依赖文件
# CC     代替  gcc
# CFLAGS 代替  编译命令


ROOT    = $(PWD)
TEST    = $(ROOT)/test
HTTP		= $(ROOT)/http
CFLAGS  = -lgtest -lgtest_main -pthread -g
OBJS    = $(TEST)/test.o $(HTTP)/http_conn.o
CC      = g++
TARGET  = $(TEST)/test.cpp $(HTTP)/http_conn.cpp
RM      = rm -rf

test:$(OBJS)
	$(CC) $(TARGET) -o $(TEST)/test  $(CFLAGS)
	$(RM) $(OBJS)

clean:
	$(RM) $(TEST)/test


