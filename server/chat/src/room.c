#include "room.h"
#include "chat.h"
#include "debug_print.h"
#include <hiredis/hiredis.h>
#include <hiredis/read.h>

room_t **_get_rooms(redisContext *redis_context) {

    DEBUG_PRINT("_get_rooms() called\n");

    static room_t **rooms = NULL;

    redisReply *reply;

    if (rooms == NULL) {
        rooms = calloc(sizeof(room_t *), MAX_ROOMS_PER_SERVER);

        reply = redisCommand(redis_context, "SMEMBERS rooms");
        if (!reply) {
            DEBUG_PRINT("redisCommand failed\n");
            free(rooms);
            return NULL;
        }
    
        for (int i = 0; i < reply->elements; i++) {
            rooms[i] = malloc(sizeof(room_t));
            rooms[i]->id = strdup(reply->element[i]->str);
            memset(rooms[i]->clients, 0,
                   sizeof(client_t *) * MAX_CLIENTS_PER_ROOM);
            rooms[i]->client_count = 0;
        }
        freeReplyObject(reply); 

        for (int i = 0; i < MAX_ROOMS_PER_SERVER && rooms[i] != NULL; i++) {
            DEBUG_PRINT("%d ITER",i);
            room_t *cur = rooms[i];
            reply = redisCommand(redis_context, "GET room_password:%s", cur->id);

            if (!reply) {
                DEBUG_PRINT("redisCommand failed\n");
                return NULL;
            }

            if (reply->type == REDIS_REPLY_STRING) {
                cur->password = strdup(reply->str);
                DEBUG_PRINT("Room %s: Password: %s",cur->id, cur->password);
            } else {
                DEBUG_PRINT("Room %s does not have a password set\n", cur->id);
                cur->password = NULL;
            }
        }
    }

    freeReplyObject(reply);
    return rooms;
}

room_t *get_room(redisContext *redis_context, const char *id) {

    room_t **rooms = _get_rooms(redis_context);

    for (int i = 0; i < MAX_ROOMS_PER_SERVER && rooms[i] != NULL; i++) {
        room_t *cur = rooms[i];
        if (strcmp(cur->id, id) == 0) {
            return cur;
        }
    }
    return NULL;
}

room_t *create_room(redisContext *redis_context, const char *id,
                    const char *password) {

    DEBUG_PRINT("create_room(%s, %s) called\n",id, password);
    redisReply *reply = redisCommand(redis_context, "SADD rooms %s", id);

    if (!reply) {
        DEBUG_PRINT("redisCommand failed\n");
        return NULL;
    }

    reply =
        redisCommand(redis_context, "SET room_password:%s %s", id, password);

    if (!reply) {
        DEBUG_PRINT("redisCommand failed\n");
        return NULL;
    }

    freeReplyObject(reply);

    // Initialization

    room_t **rooms = _get_rooms(redis_context);

    int create_room_index = 0;
    for (int i = 0; i < MAX_ROOMS_PER_SERVER; i++) {
        if (rooms[i] == NULL) {
            create_room_index = i;
            break;
        }
    }

    if (create_room_index == -1) {
        DEBUG_PRINT("No free space in rooms\n");
        return NULL;
    }

    room_t *new_room = malloc(sizeof(room_t));

    new_room->client_count = 0;
    new_room->id = strdup(id);
    if(password){
        new_room->password = strdup(password);
    }else{
        new_room->password = NULL;
    }
    
    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        new_room->clients[i] = NULL;
    }
    
    rooms[create_room_index]=new_room;
    return new_room;
}

int join_room(room_t *room, const char *password, client_t *client) {

    if (strncmp(room->password, password, strlen(room->password)) != 0) {
        DEBUG_PRINT("The invalid password attempt for the room %s\n", room->id);
        DEBUG_PRINT("%s != %s\n", room->password, password);
        return 0;
    }

    if (room->client_count >= MAX_CLIENTS_PER_ROOM) {
        DEBUG_PRINT("Too many clients for the room %s", room->id);
        return 0;
    }

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->clients[i] == NULL) {
            room->clients[i] = client;
            room->client_count++;
            return 1;
        }
    }

    return 0;
}

// Return # of left clients
int leave_room(room_t *room, client_t *client) {

    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->clients[i] == client) {
            room->clients[i] = NULL;
            break;
        }
    }

    room->client_count--;

    return room->client_count;
}

int new_message(redisContext *redis_context, const room_t *room,
                const char *msg) {

    // Save it to the history
    redisReply *reply =
        redisCommand(redis_context, "LPUSH msgs:%s %s", room->id, msg);

    if (!reply) {
        DEBUG_PRINT("redisCommand failed\n");
        return 1;
    }

    freeReplyObject(reply);

    // Broadcast to the clients in the room
    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
        if (room->clients[i] != NULL) {
            if (write(room->clients[i]->sockfd, msg, strlen(msg)) < 0) {
                DEBUG_PRINT("write() failed\n");
                return 1;
            }
        }
    }

    return 0;
}

char *get_messages(redisContext *redis_context, const room_t *room) {

    int count = 0;
    char *res = malloc(BUFF_LEN);

    if (!res) {
        DEBUG_PRINT("malloc failed\n");
        return NULL;
    }

    redisReply *reply =
        redisCommand(redis_context, "LRANGE msgs:%s 0 100", room->id);

    if (!reply) {
        DEBUG_PRINT("redisCommand failed\n");
        return NULL;
    }

    for (int i = reply->elements - 1; i >= 0; i--) {
        const char *line = reply->element[i]->str;
        strncat(res, line, BUFF_LEN - (count + 1));
        count += strlen(line);
        if (count >= BUFF_LEN - 1) {
            break;
        }
    }

    freeReplyObject(reply);
    return res;
}
