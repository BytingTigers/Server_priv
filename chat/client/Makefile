CC=gcc
CFLAGS=-g -Wall 
LDFLAGS=
OBJS=client.o
TARGET=client

all: $(TARGET) clean_object

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

client.o: test_chat_client.c
	$(CC) $(CFLAGS) -c test_chat_client.c -o client.o

clean:
	rm -f *.o
	rm -f $(TARGET) $(TARGET)

clean_object:
	rm -f *.o
.PHONY: client clean clean_object