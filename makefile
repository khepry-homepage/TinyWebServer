# OBJ   代替  依赖文件
# CC     代替  gcc

ROOT    		= $(PWD)
TEST    		= $(ROOT)/test
SERVER			= $(ROOT)/tiny_server
HTTP		 		= $(ROOT)/http
DB_CONN			= $(ROOT)/db_conn
LOG					= $(ROOT)/log
TIMER				= $(ROOT)/timer

VERSION     = 1.0
CC   				= g++
CFLAGS  		= -g -pthread
LIB_PATH  	= -L /usr/lib/x86_64-linux-gnu/
LIB_NAMES  	= -lmysqlclient
LIB_TEST		= -lgtest -lgtest_main
OBJ   			=	main.o server.o timer.o http_conn.o db_connpool.o log.o
TEST_OBJ		= test.o http_conn.o
TARGET  		=	server
RM      		=	rm -rf

$(TARGET):$(OBJ)
	$(CC) $^ -o $@ $(CFLAGS) $(LIB_PATH) $(LIB_NAMES)
	$(RM) $(OBJ)
test:$(TEST_OBJ)
	$(CC) $^ -o $@ $(LIB_PATH) $(LIB_NAMES)

# compile
main.o:$(ROOT)/main.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
http_conn.o:$(HTTP)/http_conn.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
db_connpool.o:$(DB_CONN)/db_connpool.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
timer.o:$(TIMER)/timer.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
log.o:$(LOG)/log.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
server.o:$(SERVER)/server.cpp
	$(CC) -o $@ -c $< $(CFLAGS)
test.o:$(TEST)/test.cpp
	$(CC) $(LIB_TEST) -o $@ -c $< $(CFLAGS)

.PHONY:clean
clean:
	@echo "Remove linked and compiled files......"
	$(RM) $(OBJ) $(TARGET)
