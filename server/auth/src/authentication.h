#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <stddef.h>
#define SALT_LENGTH 16
#define HASH_LENGTH SHA256_DIGEST_LENGTH

#define SECRET_KEY "OP0GVA1ABbK04hC46NkEYsBAykjUNe0dvf+COdW/YGI="

#define REDIS_HOST "localhost"
#define REDIS_PORT 6379

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auth"
#define DB_PORT 3306
// #define REDIS_PASS "bytingtigers"

char *toHexString(unsigned char *data, size_t dataLength);

char *StringtoHex(const char *hex);

void createSaltedHash(const char *password, unsigned char *salt,
                      unsigned char *hash);

int signup(const char *username, const char *password);

char *signin(const char *username, const char *password);

char *generate_jwt(const char *username);

int verify_jwt(const char *jwt_string, const char *username);
#endif
