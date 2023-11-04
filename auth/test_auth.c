#include <stdio.h>
#include <stdlib.h>
#include "authentication.h"

int main(){
    char *id, *password;
    id = calloc(sizeof(char),21);
    password = calloc(sizeof(char),31);

    if(!id || !password){
        fprintf(stderr,"calloc error");
        exit(1);
    }
    printf("signup[1] or signin[2]?\n");
    int mode;
    scanf("%d",&mode);
    if(mode == 1){
        printf("id> ");
        scanf("%30s",id);

        printf("\nPW> ");
        scanf("%30s",password);

        signup(id, password);
    }
    else{
    printf("id> ");
    scanf("%30s",id);

    printf("\nPW> ");
    scanf("%30s",password);

    char *jwt;
    if(jwt=signin(id, password)){
        printf("signin!\n");
        printf("jwt: %s",jwt);

    }
    else
        printf("signin failed");
    free(id);
    free(password);
    }
    return 0;
}
