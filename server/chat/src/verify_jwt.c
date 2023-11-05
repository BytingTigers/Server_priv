#include "verify_jwt.h"

int verify_jwt(const char *jwt_string, const char *username) {

    DEBUG_PRINT("Decode JWT\n");
    DEBUG_PRINT("Username: %s\n", username);
    DEBUG_PRINT("JWT: %s\n", jwt_string);

    jwt_t *jwt = NULL;

    int ret = jwt_decode(&jwt, jwt_string, (const unsigned char *)SECRET_KEY, strlen(SECRET_KEY));
    // int ret = jwt_decode(&jwt, jwt_string, NULL, 0);

    if (ret != 0) {
        fprintf(stderr, "Invalid JWT.\n");
        DEBUG_PRINT("Invalid JWT\n");
        jwt_free(jwt);
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
    DEBUG_PRINT("Username in JWT: %s\n",token_username);
    if (!token_username || strcmp(token_username, username) != 0) {
        fprintf(stderr, "Username mismatch. \n");
        jwt_free(jwt);
        return 0;
    }

    jwt_free(jwt);
    return 1; // JWT is valid
}
