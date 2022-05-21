# OBJS   代替  依赖文件
# CC     代替  gcc
# CFLAGS 代替  编译命令


ROOT    = $(PWD)
TEST    = $(ROOT)/test
CFLAGS  = -lgtest -lgtest_main -pthread -g
OBJS    = $(TEST)/test.o
CC      = g++
TARGET  = $(TEST)/test.cpp
RM      = rm -rf

test:$(OBJS)
	$(CC) $(TARGET) -o $(OBJS) $(CFLAGS)

clean:
	$(RM) $(OBJS)


