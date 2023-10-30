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

    printf("id> ");
    scanf("%30s",id);

    printf("\nPW> ");
    scanf("%30s",password);

    if(signin(id, password)){
        printf("signin!");
    }
    else
        printf("signin failed");
    free(id);
    free(password);

    return 0;
}
