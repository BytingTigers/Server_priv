#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <openssl/rand.h>
#include "authentication.h"

#define DB_HOST "43.201.26.232"
#define DB_USER "bytingtigers"
#define DB_PASS "bytingtigers"
#define DB_NAME "auth"

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

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
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
    return 1;
}

int signin(const char* id, const char* password){
    MYSQL *conn;
    MYSQL_RES *res;
    char query[512];

    conn = mysql_init(NULL);

    if (!conn) {
        fprintf(stderr, "mysql_init() failed\n");
        return EXIT_FAILURE;
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        return EXIT_FAILURE;
    }

    snprintf(query,sizeof(query),"SELECT salt FROM users WHERE username='%s'",id);
    // overflow possible
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    res = mysql_store_result(conn);
    char *stringSalt = mysql_fetch_row(res)[0];
    unsigned char *salt = StringtoHex(stringSalt);
    unsigned char *hash = calloc(HASH_LENGTH,sizeof(char));
    if(!hash || !salt)
        return -1;

    createSaltedHash(password, salt, hash);
    char *hashString = toHexString(hash,HASH_LENGTH);

    snprintf(query,sizeof(query),"SELECT salt FROM users WHERE username='%s' and password_hash='%s'",id,hashString);
    // overflow possible
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }
    res = mysql_store_result(conn);
    int result = mysql_num_rows(res);

    free(salt);
    free(hash);
    free(hashString);

    mysql_free_result(res);
    mysql_close(conn);

    return result==1;
}