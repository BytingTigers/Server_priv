#include "chat.h"

void add_client(client_t *cl, chat_server_t *server){
    pthread_mutex_lock(&server->clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(!server->clients[i]){
            server->clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void remove_client(int uid, chat_server_t *server){
    pthread_mutex_lock(&server->clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(server->clients[i]){
            if(server->clients[i]->uid == uid){
                server->clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void send_message(char *s, int uid, chat_server_t *server){
    pthread_mutex_lock(&server->clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(server->clients[i]){
            char full_message[1024+10+3];
            if(uid<0) // for server
                snprintf(full_message, sizeof(full_message), "SERVER: %s",s);
            else
                snprintf(full_message, sizeof(full_message), "%s: %s",server->clients[uid]->username,s);

            if(write(server->clients[i]->sockfd, full_message, strlen(full_message)) < 0){
                    perror("ERROR: write to descriptor failed");
                    break;
            }
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void *handle_client(void *arg){
    char buffer[BUFFER_SIZE];
    int leave_flag = 0;
    thread_args_t *args = (thread_args_t *)arg;
    client_t *cli = args->client;
    chat_server_t *server = args->server;

    int username_len = recv(cli->sockfd, cli->username, 10, 0);
    if(username_len <= 0){
        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());

        return NULL;
    }
    cli->username[username_len] = '\0';

    // print join message
    char join_message[10+20];
    snprintf(join_message,sizeof(join_message),"%s has joined.\n",cli->username);
    send_message(join_message,-1,server);

    // Receive data from client
    while(1){
        if (leave_flag) {
            break;
        }

        int receive = recv(cli->sockfd, buffer, BUFFER_SIZE, 0);
        if (receive > 0){
            if(strlen(buffer) > 0){
                send_message(buffer, cli->uid, server);
                memset(buffer, 0, BUFFER_SIZE);
            }
        } else if (receive == 0 || strcmp(buffer, "exit") == 0){
            sprintf(buffer, "%d has left\n", cli->uid);
            printf("%s", buffer);
            send_message(buffer, cli->uid, server);
            leave_flag = 1;
        } else {
            perror("ERROR: -1");
            leave_flag = 1;
        }
    }

    close(cli->sockfd);
    remove_client(cli->uid, server);
    free(cli);
    free(args);
    pthread_detach(pthread_self());

    return NULL;
}

void print_client_addr(struct sockaddr_in addr){
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("%s", client_ip);
}

void *start_chat_server(void *port){
    int server_port = *(int*)port;
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
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("ERROR: Socket binding failed");
        return NULL;
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return NULL;
    }

    printf("<[ SERVER at PORT %d STARTED ]>\n",server_port);

    // Accept clients
    while(1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check if max clients is reached
        if((client_count + 1) == MAX_CLIENTS){
            printf("Max clients reached. Rejected: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
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

        // Add client to the array and fork thread
        add_client(cli, &server);
        if (pthread_create(&tid, NULL, &handle_client, (void*)args) != 0) {
            perror("Failed to create thread");
            free(cli);
            free(args);
            continue;
        }

        client_count++;
        printf("<[ SERVER at PORT %d: %d/%d ]>\n",server_port,client_count,MAX_CLIENTS);
        
        // Reduce CPU usage
        sleep(1);
    }
    pthread_mutex_destroy(&server.clients_mutex);
    close(listenfd);

    return NULL;
}