#include "authentication.h"
#include "debug_print.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <hiredis/hiredis.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CLIENT_TIMEOUT 30
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

const char reply[] = "ERROR_QUIT";

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

    char buffer[BUFFER_SIZE];
    int leave_flag = 0;

    thread_args_t *args = (thread_args_t *)arg;
    client_t *cli = args->client;
    chat_server_t *server = args->server;
    redisContext *redis_context = args->redis_context;

    DEBUG_PRINT(
        "[Client: %d] A new thread created for handling the new client.\n",
        cli->uid);

    // set mode sent via socket
    int recv_len = recv(cli->sockfd, &buffer, sizeof(buffer), 0);
    DEBUG_PRINT("[Client: %d] Data received: %s\n", cli->uid, buffer);

    if (recv_len <= 0) {

        if (recv_len < 0 && errno == ETIMEDOUT) {
            DEBUG_PRINT("[Client: %d] Timeout.\n", cli->uid);
        }

        DEBUG_PRINT("[Client: %d] Client disconnected.\n", cli->uid);
        goto close_conn;
    }

    char *token;
    char *rest = buffer;
    int mode;
    char id[30];
    char pw[20];
    const char delim[] = ".";

    token = strtok_r(rest, delim, &rest);
    if (token != NULL) {
        mode = atoi(token);
    } else {
        send(cli->sockfd, reply, sizeof(reply), NULL);
        goto close_conn;
    }

    token = strtok_r(NULL, delim, &rest);
    if (token != NULL) {
        strncpy(id, token, 30 - 1);
        id[30 - 1] = '\0';
    } else {
        send(cli->sockfd, reply, sizeof(reply), NULL);
        goto close_conn;
    }

    token = strtok_r(NULL, delim, &rest);
    if (token != NULL) {
        strncpy(pw, token, 20 - 1);
        pw[20 - 1] = '\0';
    } else {
        send(cli->sockfd, reply, sizeof(reply), NULL);
        goto close_conn;
    }

    DEBUG_PRINT("[Client: %d] ID: `%s`, pw: `%s`\n", cli->uid, id, pw);

    switch (mode) {
    case 1:

        DEBUG_PRINT("[Client: %d] Mode: %d\n", cli->uid, mode);

        pthread_mutex_lock(&server->clients_mutex);
        if (signup(id, pw)) {
            send(cli->sockfd, reply, sizeof(reply), NULL);
        } else {
            const char reply[] = "SUCCESS";
            send(cli->sockfd, reply, sizeof(reply), NULL);
        }
        pthread_mutex_unlock(&server->clients_mutex);

        break;

    case 2:

        pthread_mutex_lock(&server->clients_mutex);
        const char *jwt = signin(id, pw);
        pthread_mutex_unlock(&server->clients_mutex);
        if (jwt == NULL) {
            send(cli->sockfd, "ERROR", 6, 0);
        } else {
            send(cli->sockfd, jwt, strlen(jwt) + 1, 0);
        }

        break;
    }

close_conn:

    close(cli->sockfd);
    remove_client(cli->uid, server);
    free(cli);
    free(args);
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv) {

    struct timeval timeout;
    timeout.tv_sec = CLIENT_TIMEOUT;
    timeout.tv_usec = 0;

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

    redisCommand(redis_context, "SELECT 2"); // Database 2 is for auth.

    DEBUG_PRINT("The connection to Redis is established.\n");

    // Event loops
    while (1) {

        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                       sizeof(timeout)) < 0) {
            DEBUG_PRINT("setsockopt() failed");
            break;
        }

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
