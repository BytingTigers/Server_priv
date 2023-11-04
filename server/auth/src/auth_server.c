#include "authentication.h"
#include "debug_print.h"
#include <arpa/inet.h>
#include <hiredis/hiredis.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

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
    char title[30];
    char password[20];
} chat_server_t;

typedef struct {
    client_t *client;
    chat_server_t *server;
    redisContext *redis_context;
} thread_args_t;

void add_client(client_t *cl, chat_server_t *server) {
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!server->clients[i]) {
            server->clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void remove_client(int uid, chat_server_t *server) {
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (server->clients[i]) {
            if (server->clients[i]->uid == uid) {
                server->clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void *handle_client(void *arg) {

    DEBUG_PRINT("A new thread created for handling the new client.\n");

    char buffer[BUFFER_SIZE];
    int leave_flag = 0;

    thread_args_t *args = (thread_args_t *)arg;
    client_t *cli = args->client;
    chat_server_t *server = args->server;
    redisContext *redis_context = args->redis_context;

    // set mode sent via socket
    int recv_len = recv(cli->sockfd, &buffer, sizeof(buffer), 0);

    if (recv_len <= 0) {
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());
    }

    int mode = atoi(buffer);

    char IDPW[20 + 30 + 2];
    char ID[20] = {0}, PW[30] = {0};
    char *token;

    recv_len = recv(cli->sockfd, IDPW, sizeof(IDPW) - 1, 0);
    DEBUG_PRINT("Data received: %s\n", IDPW);
    if (recv_len <= 0) {
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());
    }

    IDPW[recv_len] = '\0';
    token = strtok(IDPW, ".");
    if (token != NULL) {
        strncpy(ID, token, sizeof(ID) - 1);
        token = strtok(NULL, ".");
        if (token != NULL) {
            strncpy(PW, token, sizeof(PW) - 1);
        }
    }

    DEBUG_PRINT("ID: %s\nPW: %s\n", ID, PW);
    DEBUG_PRINT("Mode: %d\n", mode);

    if (mode == 1) { // mode 1 is signup
        if (signup(ID, PW)) {
            send(cli->sockfd, "ERROR", 6, 0);
        } else {
            send(cli->sockfd, "SUCCESS", 7, 0);
        }
    } else if (mode == 2) { // mode 2 is signin
        char *jwt;
        jwt = signin(ID, PW);
        if (jwt == NULL) {
            send(cli->sockfd, "ERROR", 6, 0);
        } else {
            send(cli->sockfd, jwt, strlen(jwt) + 1, 0);
        }
    }

    close(cli->sockfd);
    remove_client(cli->uid, server);
    free(cli);
    free(args);
    pthread_detach(pthread_self());
}

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stdout, "Usage: start [port]\n");
        return -1;
    }

    int server_port = atoi(argv[1]);

    if (server_port == 0) {
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, sizeof(err_msg),
                 "Cound not convert to a port number: %s", argv[1]);
        perror(err_msg);
    }

    chat_server_t server;

    pthread_mutex_init(&server.clients_mutex, NULL);
    memset(server.clients, 0, sizeof(server.clients));
    server.server_port = server_port;

    int client_count = 0;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server.server_port);

    // Bind
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return -1;
    }

    DEBUG_PRINT("Bind succeeded.\n");

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return -1;
    }

    DEBUG_PRINT("Listen started.\n");

    // REDIS SETUP

    // Connect to Redis server
    redisContext *redis_context = redisConnect(REDIS_HOST, REDIS_PORT);
    if (redis_context == NULL || redis_context->err) {
        if (redis_context) {
            printf("Error: %s\n", redis_context->errstr);
            redisFree(redis_context);
        } else {
            printf("Can't allocate redis context\n");
        }
        exit(1);
    }

    printf("<[ AUTH SERVER at PORT %d STARTED ]>\n", server_port);

    // change database to JWT(DB 2)
    redisCommand(redis_context, "SELECT 2");

    // Accept clients
    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        DEBUG_PRINT("The connection accepted.\n");

        // Check if max clients is reached
        if ((client_count + 1) == MAX_CLIENTS) {
            close(connfd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        if (cli == NULL) {
            perror("Failed to allocate memory for new client");
            continue;
        }

        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = client_count; // Use client_count as unique ID

        // Prepare arguments for thread
        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (args == NULL) {
            perror("Failed to allocate memory for thread arguments");
            free(cli);
            continue;
        }
        args->client = cli;
        args->server = &server;
        args->redis_context = redis_context;

        // Add client to the array and fork thread
        add_client(cli, &server);
        if (pthread_create(&tid, NULL, &handle_client, (void *)args) != 0) {
            perror("Failed to create thread");
            free(cli);
            free(args);
            continue;
        }

        client_count++;
        // Reduce CPU usage
        sleep(1);
    }
    pthread_mutex_destroy(&server.clients_mutex);
    close(listenfd);

    return 0;
}
