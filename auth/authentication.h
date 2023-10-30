#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <openssl/sha.h>

#define SALT_LENGTH 16
#define HASH_LENGTH SHA256_DIGEST_LENGTH

char* toHexString(unsigned char* data, size_t dataLength);

char* StringtoHex(const char* hex);

void createSaltedHash(const char *password, unsigned char *salt, unsigned char *hash);

int signup(const char* username, const char* password);

int signin(const char* username, const char* password);

#endif