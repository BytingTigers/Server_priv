#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1" // Change to your server's IP address
#define BUFFER_SIZE 1024

int sockfd;

int main(){
    struct sockaddr_in server_addr;

    // Creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    char *id, *password;
    id = calloc(sizeof(char),21);
    password = calloc(sizeof(char),31);

    if(!id || !password){
        fprintf(stderr,"calloc error");
        exit(1);
    }
    printf("signup[1] or signin[2]?\n");

    int mode;
    char jwt[200];
    char IDPW[52];
    scanf("%d",&mode);
    
    if(mode == 1){
        printf("id> ");
        scanf("%20s",id);

        printf("\nPW> ");
        scanf("%30s",password);

        mode = htonl(mode);
        send(sockfd, &mode, sizeof(int), 0); // mode sent
        snprintf(IDPW,sizeof(IDPW),"%s.%s",id,password);
        send(sockfd,IDPW, sizeof(IDPW), 0);
        char return_val[20];
        int recv_len = recv(sockfd,return_val,sizeof(return_val),0);
        return_val[recv_len]='\0';
        printf("%s",return_val);
    }
    else if(mode == 2){
        printf("id> ");
        scanf("%20s",id);

        printf("\nPW> ");
        scanf("%30s",password);

        mode = htonl(mode);
        send(sockfd, &mode, sizeof(int), 0); // mode sent
        snprintf(IDPW,sizeof(IDPW),"%s.%s",id,password);
        send(sockfd,IDPW, sizeof(IDPW), 0);
        int recv_len = recv(sockfd,jwt,sizeof(jwt),0);
        jwt[recv_len]='\0';

        if(jwt){
            printf("signin!\n");
            printf("jwt: %s",jwt);

        }
        else
            printf("signin failed");
    }
    free(id);
    free(password);
    return 0;
}
