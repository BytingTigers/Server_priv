#include <stdio.h>
#include <stdlib.h>

#include "debug_print.h"
#include "chat.h"

#define MAX_CHATROOMS 100

void display_help() {
    fprintf(stdout, "Usage: start [port]\n");
}

void *handle_main_client(void *arg){
    char buffer[BUFFER_SIZE];
    int recv_len = 0;
    int chatroom_count = 0;
    chat_server_t *chatrooms[MAX_CHATROOMS];

    int server_port[MAX_CHATROOMS];
    for(int i=0; i< MAX_CHATROOMS; i++){
        server_port[i] = 5010+i;
    }

    thread_args_t *args = (thread_args_t *)arg;
    client_t *cli = args->client;
    chat_server_t *server = args->server;
    redisContext *redis_context = args->redis_context;

    char username[21]={0};
    recv_len = recv(cli->sockfd, username, 20, 0);
    username[recv_len] = '\0';
    // authenticate JWT sent via socket
    recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
    buffer[recv_len]='\0';
    redisCommand(redis_context, "SELECT 2");

    redisReply *reply = redisCommand(redis_context, "GET %s", username);
    if(reply == NULL){
        send(cli->sockfd, "ERROR", 5, 0);
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());
    }
    else if(strcmp(reply->str, buffer) != 0){
        send(cli->sockfd, "ERROR", 5, 0);
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());
        }
    else{
        send(cli->sockfd, "SUCCESS", 7, 0);
    }
    // set mode sent via socket
    recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
    if(recv_len <= 0) {
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());
    }

    buffer[recv_len] = '\0';
    int mode = atoi(buffer);
    DEBUG_PRINT("MODE RECEIVED %d\n",mode);
    switch(mode){
        case 1: // make chatroom
            if(chatroom_count >= MAX_CHATROOMS) {
                send(cli->sockfd, "Server capacity reached. Cannot create new chatroom.", 55, 0);
            } else {
                chatrooms[chatroom_count] = malloc(sizeof(chat_server_t));
                if (chatrooms[chatroom_count] == NULL) {
                    send(cli->sockfd, "Server error. Try again later.", 30, 0);
                } else {
                    // Initialize and start a new chat server for the new chatroom
                    chat_server_t *new_chatroom = chatrooms[chatroom_count];
                    pthread_mutex_init(&new_chatroom->clients_mutex, NULL);
                    memset(new_chatroom->clients, 0, sizeof(new_chatroom->clients));
                    new_chatroom->server_port = server_port[chatroom_count];

                    // set title
                    recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
                    buffer[recv_len] = '\0';
                    strncpy(new_chatroom->title, buffer, sizeof(buffer));
                    // set password
                    recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
                    buffer[recv_len] = '\0';
                    strncpy(new_chatroom->password, buffer, sizeof(buffer));

                    pthread_t chatroom_thread;
                    if(pthread_create(&chatroom_thread, NULL, &start_chat_server, (void*)&server_port[chatroom_count]) != 0) {
                        send(cli->sockfd, "Failed to create chatroom. Try again later.", 42, 0);
                    } else {
                        chatroom_count++;
                        send(cli->sockfd, "Chatroom created successfully.", 30, 0);
                    }
                }
            }
            break;
        case 2: // list chatroom
            char list_buffer[BUFFER_SIZE];
            int offset = 0;
            for(int i = 0; i < chatroom_count; i++) {
                offset += snprintf(list_buffer + offset, BUFFER_SIZE - offset, "Chatroom %d: Port %d\n", i+1, server_port[i]);
            }
            if(chatroom_count == 0) {
                send(cli->sockfd, "No chatrooms available.", 23, 0);
            } else {
                send(cli->sockfd, list_buffer, strlen(list_buffer), 0);
            }
            break;
        // case 3: // join chatroom
        //     char join_buffer[BUFF_LEN];
        //     recv_len = recv(cli->sockfd, join_buffer, sizeof(join_buffer), 0);
        //     if(recv_len <= 0) {
        //         send(cli->sockfd, "Failed to receive chatroom info.", 31, 0);
        //     } else {
        //         join_buffer[recv_len] = '\0';
        //         int chatroom_number = atoi(join_buffer) - 1;
        //         if(chatroom_number < 0 || chatroom_number >= chatroom_count) {
        //             send(cli->sockfd, "Invalid chatroom number.", 24, 0);
        //         } else {
        //             // Logic to join the chatroom would go here.
        //             // This could be sending back the chatroom details, connecting the client to the chatroom's socket, etc.
        //             snprintf(join_buffer, sizeof(join_buffer), "Connect to chatroom on port %d.", server_port[chatroom_number]);
        //             send(cli->sockfd, join_buffer, strlen(join_buffer), 0);
        //         }
        //     }
        //     break;

    }
    close(cli->sockfd);
    remove_client(cli->uid, server);
    free(cli);
    free(args);
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        display_help();
        return -1;
    }

    int server_port = atoi(argv[1]);

    if (server_port == 0) {
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, sizeof(err_msg), "Cound not convert to a port number: %s", argv[1]);
        perror(err_msg);
        return -1;
    }

    if(server_port < 1 || server_port > 65535) {
        printf("Invalid port: %d\n", server_port);
        return -1;
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
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        exit(1);
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        exit(1);
    }

    printf("<[ MESSAGE SERVER at PORT %d STARTED ]>\n",server_port);

    // REDIS SETUP

    // Connect to Redis server
    redisContext *redis_context = redisConnect(REDIS_SERVER, REDIS_PORT);
    if (redis_context == NULL || redis_context->err) {
        if (redis_context) {
            printf("Error: %s\n", redis_context->errstr);
            redisFree(redis_context);
        } else {
            printf("Can't allocate redis context\n");
        }
        exit(1);
    }

    // change database to message(DB 1)
    redisCommand(redis_context, "SELECT 1");

    // Accept clients
    while(1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check if max clients is reached
        if((client_count + 1) == MAX_CLIENTS) {
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
        if (pthread_create(&tid, NULL, &handle_main_client, (void*)args) != 0) {
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
