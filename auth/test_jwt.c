#include <jwt.h>
#include <time.h>
#include <jansson.h>
#include <string.h>

#define SECRET_KEY "OP0GVA1ABbK04hC46NkEYsBAykjUNe0dvf+COdW/YGI="

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


int main(){
    char* jwt = generate_jwt("username");
    if(verify_jwt(jwt, "username")){
        printf("VALID");
    }
    else{
        printf("INVALID");
    }
    return 0;
}