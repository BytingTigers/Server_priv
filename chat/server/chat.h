#ifndef CHAT_H
#define CHAT_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <hiredis/hiredis.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char username[10];
} client_t;

typedef struct {
    client_t *clients[MAX_CLIENTS];
    pthread_mutex_t clients_mutex;
    int server_port;
} chat_server_t;

typedef struct {
    client_t *client;
    chat_server_t *server;
    redisContext *redis_context;
} thread_args_t;

void add_client(client_t *cl, chat_server_t *server);
void remove_client(int uid, chat_server_t *server);
void send_message(char *s, int uid, chat_server_t *server, redisContext *redis_context);
void *handle_client(void *arg);
void print_client_addr(struct sockaddr_in addr);
void *start_chat_server(void *port);

#endif //CHAT_H
