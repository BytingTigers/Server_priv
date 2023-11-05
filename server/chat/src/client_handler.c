#include "client_handler.h"
#include <unistd.h>

/* Remove non-ASCII characters and ASCII control characters from the received
 * string. */
static char *sanitize(const char *str, size_t len) {
    static char buffer[BUFF_LEN + 2];
    unsigned int i, j;

    if (len > BUFF_LEN) {
        fprintf(stderr, "ERROR: message too long\n");
        return NULL;
    }

    memset(buffer, 0, sizeof(buffer));

    for (i = 0, j = 0; i < len; i++) {
        if (str[i] < 0x20 || str[i] > 0x7e)
            continue;
        buffer[j] = str[i];
        j++;
    }

    /* Add a line feed at the end of non-empty strings */
    if (j > 0)
        buffer[j] = 0x0a;

    return buffer;
}

void *handle_client(void *arg) {
    char buffer[BUFF_LEN];
    int leave_flag = 0;
    int recv_len;

    char *token, *rest;
    char username[MAX_USERNAME_LEN];
    char jwt_string[BUFF_LEN];
    const char delim[] = ":";

    char room_id[MAX_ROOM_ID_LEN];
    char password[MAX_PASSWORD_LEN];

    const char error_msg[] = "ERROR";

    thread_args_t *args = (thread_args_t *)arg;

    client_t *cli = args->client;
    chat_server_t *server = args->server;
    redisContext *redis_context = args->redis_context;

    request_t request;
    room_t *room = NULL;

    // ######################
    // # Token verification #
    // ######################
    recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
    if (recv_len <= 0) {

        DEBUG_PRINT("The client disconnected\n");

        close(cli->sockfd);
        remove_client(cli->uid, server);
        free(cli);
        free(args);
        pthread_detach(pthread_self());

        return NULL;
    }

    sanitize(buffer, strlen(buffer));

    DEBUG_PRINT("received: %s\n", buffer);

    // `auth`
    rest = buffer;
    token = strtok_r(rest, delim, &rest);
    if (token == NULL) {
        DEBUG_PRINT("Invalid format.\n");
        if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
            DEBUG_PRINT("send() failed");
        }
        return NULL;
    }

    if (strncmp(token, "auth", strlen(token)) != 0) {
        DEBUG_PRINT("Only auth is allowed for guests.\n");
        if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
            DEBUG_PRINT("send() failed");
        }
        return NULL;
    }

    // Username
    token = strtok_r(NULL, delim, &rest);
    if (token == NULL) {
        DEBUG_PRINT("Invalid format.\n");
        if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
            DEBUG_PRINT("send() failed");
        }
        return NULL;
    }
    strncpy(username, token, MAX_USERNAME_LEN);


    // Username
    token = strtok_r(NULL, delim, &rest);
    if (token == NULL) {
        DEBUG_PRINT("Invalid format.\n");
        if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
            DEBUG_PRINT("send() failed");
        }
        return NULL;
    }
    strncpy(jwt_string, token, BUFF_LEN);

    if (verify_jwt(jwt_string, username) == 0) {
        DEBUG_PRINT("Invalid jwt.\n");
        if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
            DEBUG_PRINT("send() failed");
        }
        return NULL;
    }
    strncpy(cli->username, username, MAX_USERNAME_LEN - 1);
    cli->username[MAX_USERNAME_LEN - 1] = '\0';

    // ##################
    // # Message handle #
    // ##################

    while (1) {

        // set username sent via socket
        recv_len = recv(cli->sockfd, buffer, sizeof(buffer), 0);

        if (recv_len <= 0) {

            DEBUG_PRINT("The client disconnected\n");

            close(cli->sockfd);
            remove_client(cli->uid, server);
            free(cli);
            free(args);
            pthread_detach(pthread_self());

            return NULL;
        }
        buffer[recv_len]='\0';
        sanitize(buffer, strlen(buffer));

        DEBUG_PRINT("received: %s\n", buffer);

        rest = buffer;
        token = strtok_r(rest, delim, &rest);
        if (token == NULL) {
            DEBUG_PRINT("Invalid format.\n");
            if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                DEBUG_PRINT("send() failed");
            }
            return NULL;
        }

        if (strncmp("join", token, 4) == 0) {
            request = JOIN;
        } else if (strncmp("leave", token, 5) == 0) {
            request = LEAVE;
        } else if (strncmp("send", token, 4) == 0) {
            request = SEND;
        } else if (strncmp("make", token, 4) == 0) {
            request = MAKE;
        } else if (strncmp("quit", token, 4) == 0) {
            request = QUIT;
        } else if (strncmp("list", token, 4) == 0) {
            request = LISTROOM;
        } else{
            continue;
        }

        switch (request) {
        case JOIN:

            DEBUG_PRINT("JOIN\n");

            token = strtok_r(NULL, delim, &rest);
            if (token == NULL) {
                DEBUG_PRINT("Invalid format.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                return NULL;
            }
            strncpy(room_id, token, MAX_ROOM_ID_LEN);

            token = strtok_r(NULL, delim, &rest);
            if (token == NULL) {
                DEBUG_PRINT("Invalid format.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                return NULL;
            }
            strncpy(password, token, sizeof(password));

            if (room != NULL) {
                leave_room(room, cli);
            }

            room = get_room(redis_context, room_id);

            if (room == NULL) {
                DEBUG_PRINT("Room %s does not exist.\n", room_id);
                break;
            }

            join_room(room, password, cli);

            snprintf(buffer, BUFF_LEN, "%s joined the room!\n", cli->username);

            // load messages
            redisReply *reply = redisCommand(redis_context, "LRANGE msgs:%s -10 -1",room_id);
            DEBUG_PRINT("%d messages loaded from redis\n",reply->elements);
            for(int i=reply->elements - 1;i >= 0;i--){
                send(cli->sockfd, reply->element[i]->str, strlen(reply->element[i]->str), 0);
            }
            if (new_message(redis_context, room, buffer) == 1) {
                DEBUG_PRINT("new_message() failed\n");
            }

            break;

        case LEAVE:

            DEBUG_PRINT("LEAVE\n");

            if (room != NULL) {
                leave_room(room, cli);
            }
            room = NULL;
            break;

        case SEND:

            DEBUG_PRINT("SEND\n");

            if (room == NULL) {
                DEBUG_PRINT("Cannot send a message before joining.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                close(cli->sockfd);
                remove_client(cli->uid, server);
                free(cli);
                free(args);
                pthread_detach(pthread_self());
                return NULL;
            }

            token = strtok_r(NULL, delim, &rest);
            if (token == NULL) {
                DEBUG_PRINT("Invalid format.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                close(cli->sockfd);
                remove_client(cli->uid, server);
                free(cli);
                free(args);
                pthread_detach(pthread_self());
                return NULL;
            }

            snprintf(buffer, BUFFER_SIZE, "%s: %s",username, token);
            buffer[sizeof(buffer) - 1] ='\0';
            DEBUG_PRINT("new MESSAGE arrived: %s\n",buffer);
            new_message(redis_context, room, buffer);
            break;

        case MAKE:

            DEBUG_PRINT("MAKE\n");

            token = strtok_r(NULL, delim, &rest);
            if (token == NULL) {
                DEBUG_PRINT("Invalid format.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                return NULL;
            }
            strncpy(room_id, token, sizeof(room_id));

            token = strtok_r(NULL, delim, &rest);
            if (token == NULL) {
                DEBUG_PRINT("Invalid format.\n");
                if (send(cli->sockfd, error_msg, strlen(error_msg), 0) < 0) {
                    DEBUG_PRINT("send() failed");
                }
                return NULL;
            }
            strncpy(password, token, sizeof(password));

            DEBUG_PRINT("Room ID: %s\n", room_id);
            DEBUG_PRINT("Password: %s\n", password);

            create_room(redis_context, room_id, password);
            DEBUG_PRINT("Room created: %s\n", room_id);
            break;

        case QUIT:
            close(cli->sockfd);
            remove_client(cli->uid, server);
            free(cli);
            free(args);
            pthread_detach(pthread_self());
            return NULL;

        case LISTROOM:

            DEBUG_PRINT("LIST\n");

            reply = redisCommand(redis_context, "SMEMBERS rooms");
            memset(buffer, 0, sizeof(buffer));
            int count = reply -> elements;
            int current_len = 0;
            for (int i = 0; i < count; i++) {
                char* room_id = strdup(reply -> element[i]->str);
                int len = strlen(room_id);
                if (current_len + len < BUFF_LEN) {
                    strcat(buffer, room_id);
                    strcat(buffer,".");
                    current_len += len + 1;
                }
                free(room_id);
            }
            buffer[current_len]='\0';
            DEBUG_PRINT("CHAT LIST: %s\n",buffer);
            send(cli->sockfd, buffer, strlen(buffer), 0);
        }
    }

    close(cli->sockfd);
    remove_client(cli->uid, server);
    free(cli);
    free(args);
    pthread_detach(pthread_self());

    return NULL;
}
