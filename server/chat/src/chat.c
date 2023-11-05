#include "chat.h"

void *start_chat_server(void *port) {
    int server_port = *(int *)port;
    chat_server_t server;

    pthread_mutex_init(&server.clients_mutex, NULL);
    memset(server.clients, 0, sizeof(server.clients));
    server.server_port = server_port;

    struct timeval timeout;
    timeout.tv_sec = CLIENT_TIMEOUT;
    timeout.tv_usec = 0;

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
        return NULL;
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return NULL;
    }

    printf("<[ SERVER at PORT %d STARTED ]>\n", server_port);

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

    // change database to message(DB 1)
    redisCommand(redis_context, "SELECT 1");

    // Accept clients
    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                       sizeof(timeout)) < 0) {
            fprintf(stderr, "setsockopt() failed.\n");
            break;
        }


        client_t *cli = (client_t *)malloc(sizeof(client_t));
        if (cli == NULL) {
            perror("Failed to allocate memory for new client");
            continue;
        }

        cli->address = cli_addr;
        cli->sockfd = connfd;

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
        printf("<[ SERVER at PORT %d: %d/%d ]>\n", server_port, client_count,
               MAX_CLIENTS);
    }
    pthread_mutex_destroy(&server.clients_mutex);
    close(listenfd);

    return NULL;
}
