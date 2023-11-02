#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#define SALT_LENGTH 16
#define HASH_LENGTH SHA256_DIGEST_LENGTH

#define SECRET_KEY "OP0GVA1ABbK04hC46NkEYsBAykjUNe0dvf+COdW/YGI="

#define DB_HOST "home.hokuma.pro"
#define DB_USER "bytingtigers"
#define DB_PASS "bytingtigers"
#define DB_NAME "auth"
#define DB_PORT 6381
#define REDIS_PASS "bytingtigers"

char* toHexString(unsigned char* data, size_t dataLength);

char* StringtoHex(const char* hex);

void createSaltedHash(const char *password, unsigned char *salt, unsigned char *hash);

int signup(const char* username, const char* password);

char* signin(const char* username, const char* password);

char* generate_jwt(const char* username);

int verify_jwt(const char* jwt_string, const char* username);
#endif