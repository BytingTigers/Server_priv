#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <jwt.h>
#include <jansson.h>
#include "authentication.h"

char* toHexString(unsigned char* data, size_t dataLength) {
    char *hexString = calloc(dataLength * 2 + 1, sizeof(char));

    if (!hexString) {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }

    for (size_t i = 0; i < dataLength; i++) {
        sprintf(hexString + (i * 2), "%02x", data[i]);
    }

    hexString[dataLength * 2] = '\0';
    return hexString;
}

char* StringtoHex(const char* hex) {
    if(!hex) return NULL;

    size_t hexLen = strlen(hex) / 2;

    char *hexString = calloc(hexLen + 1, sizeof(char));

    if (!hexString) {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }

    for (size_t i = 0; i < hexLen; i++) {
        sscanf(hex+i*2,"%2hhx",&hexString[i]);
    }

    hexString[hexLen] = '\0';
    return hexString;
}

void createSaltedHash(const char *password, unsigned char *salt, unsigned char *hash){
    unsigned char data[SALT_LENGTH + strlen(password)];

    memcpy(data, salt, SALT_LENGTH);
    memcpy(data+SALT_LENGTH, password, strlen(password));

    SHA256(data, SALT_LENGTH + strlen(password), hash);
}


int signup(const char* username, const char* password){
    MYSQL *conn;
    MYSQL_RES *res;
    char query[512];

    conn = mysql_init(NULL);

    if (!conn) {
        fprintf(stderr, "mysql_init() failed\n");
        return EXIT_FAILURE;
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        return EXIT_FAILURE;
    }

    // existing username check
    snprintf(query,sizeof(query),"SELECT * FROM users WHERE username='%s'",username);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }
    res = mysql_store_result(conn);
    int num_rows = mysql_num_rows(res);
    if(num_rows) return -1;

    // create and store salt, hash
    unsigned char *salt, *password_hash;
    salt = calloc(sizeof(char), SALT_LENGTH);
    password_hash = calloc(sizeof(char), HASH_LENGTH);

    if(!salt || !password_hash){
        fprintf(stderr,"calloc error");
        exit(1);
    }

    if(!RAND_bytes(salt, SALT_LENGTH)){
        fprintf(stderr, "error generating salt.\n");
        exit(1);
    }

    createSaltedHash(password, salt, password_hash);

    char* hex_salt = toHexString(salt,SALT_LENGTH);
    char* hex_hash = toHexString(password_hash,HASH_LENGTH);

    snprintf(query,sizeof(query),"INSERT INTO users(username, password_hash, salt) values('%s','%s','%s')",username,hex_hash, hex_salt);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    free(salt);
    free(password_hash);
    free(hex_salt);
    free(hex_hash);
    mysql_close(conn);
    mysql_free_result(res);
    return 0;
}

char* signin(const char* id, const char* password){
    MYSQL *conn;
    MYSQL_RES *res;
    char query[512];

    conn = mysql_init(NULL);

    if (!conn) {
        fprintf(stderr, "mysql_init() failed\n");
        return NULL;
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        return NULL;
    }

    snprintf(query,sizeof(query),"SELECT salt FROM users WHERE username='%s'",id);
    // overflow possible
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return NULL;
    }

    res = mysql_store_result(conn);
    char *stringSalt = mysql_fetch_row(res)[0];
    unsigned char *salt = StringtoHex(stringSalt);
    unsigned char *hash = calloc(HASH_LENGTH,sizeof(char));
    if(!hash || !salt)
        return NULL;

    createSaltedHash(password, salt, hash);
    char *hashString = toHexString(hash,HASH_LENGTH);

    snprintf(query,sizeof(query),"SELECT salt FROM users WHERE username='%s' and password_hash='%s'",id,hashString);
    // overflow possible
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return NULL;
    }
    res = mysql_store_result(conn);
    int result = mysql_num_rows(res);
    mysql_free_result(res);

    // jwt generation
    redisContext *redis_context = redisConnect("home.hokuma.pro",6380);
    if (redis_context == NULL || redis_context->err) {
        if (redis_context) {
            printf("Error: %s\n", redis_context->errstr);
            redisFree(redis_context);
        } else {
            printf("Can't allocate redis context\n");
        }
        free(salt);
        free(hash);
        free(hashString);
        mysql_close(conn);

        return NULL;
    }
    
    // redis authentication
    redisReply *reply;
    reply = redisCommand(redis_context, "AUTH %s", REDIS_PASS);

    if (reply == NULL) {
        printf("Error: %s\n", redis_context->errstr);

        redisFree(redis_context);
        free(salt);
        free(hash);
        free(hashString);
        mysql_close(conn);
        return NULL;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        printf("Authentication failed: %s\n", reply->str);

        redisFree(redis_context);
        free(salt);
        free(hash);
        free(hashString);
        mysql_close(conn);
        return NULL;
    }

    freeReplyObject(reply);

    // change database to JWT(2)
    redisCommand(redis_context, "SELECT 2");
    char *jwt = NULL;

    // check if jwt exists
    reply = redisCommand(redis_context, "EXISTS %s", id);
    if(reply->integer){   
        freeReplyObject(reply);

        reply = redisCommand(redis_context, "GET %s", id);    
        jwt = strdup(reply->str);

        freeReplyObject(reply);
        redisFree(redis_context);
        free(salt);
        free(hash);
        free(hashString);
        mysql_close(conn);

        return jwt;
    }else{
        jwt = strdup(generate_jwt(id));
        reply = redisCommand(redis_context, "SETEX %s %d %s", id, 3600, jwt);
        if (reply == NULL) {
            printf("Failed to save jwt to Redis: %s\n", redis_context->errstr);
            redisFree(redis_context);
            free(salt);
            free(hash);
            free(hashString);
            mysql_close(conn);
            return NULL;

        } else {
            freeReplyObject(reply);
        }
        redisFree(redis_context);
        free(salt);
        free(hash);
        free(hashString);
        mysql_close(conn);

        return jwt;
    }
}

char* generate_jwt(const char* username) {
    jwt_t *jwt = NULL;
    char *out = NULL;
    time_t exp = time(NULL) + 3600; // Token will expire after 1 hour

    if (jwt_new(&jwt) != 0) {
        fprintf(stderr, "Error creating JWT object.\n");
        return NULL;
    }

    // Add payload
    jwt_add_grant(jwt, "username", username);
    jwt_add_grant_int(jwt, "exp", exp);

    // Generate JWT
    jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)SECRET_KEY, 32);
    out = jwt_encode_str(jwt);

    if (!out) {
        fprintf(stderr, "Error encoding JWT.\n");
        jwt_free(jwt);
        return NULL;
    }

    jwt_free(jwt);
    return out;
}

int verify_jwt(const char* jwt_string, const char* username) {
    jwt_t *jwt = NULL;
    int ret = jwt_decode(&jwt, jwt_string, (unsigned char *)SECRET_KEY, 32);

    if (ret != 0) {
        fprintf(stderr, "Invalid JWT.\n");
        return 0; // JWT is not valid
    }

    // Check for expiration
    time_t exp = jwt_get_grant_int(jwt, "exp");
    if (exp < time(NULL)) {
        fprintf(stderr, "JWT has expired.\n");
        jwt_free(jwt);
        return 0; // JWT has expired
    }

    const char *token_username = jwt_get_grant(jwt, "username");
    if(!token_username || strcmp(token_username, username) != 0){
        fprintf(stderr, "Username mismatch. \n");
        jwt_free(jwt);
        return 0;
    }

    jwt_free(jwt);
    return 1; // JWT is valid
}