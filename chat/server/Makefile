CC=gcc
CFLAGS=-g -Wall 
LDFLAGS=-lpthread -lhiredis
OBJS=server.o chat.o
TARGET=server

all: $(TARGET) clean_object

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

server.o: test_chat_server.c chat.h 
	$(CC) $(CFLAGS) -c test_chat_server.c -o server.o

chat.o: chat.c chat.h
	$(CC) $(CFLAGS) -c chat.c

clean:
	rm -f *.o
	rm -f $(TARGET) $(CLIENT_TARGET)

clean_object:
	rm -f *.o
.PHONY: server clean clean_object