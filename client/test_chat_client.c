#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1" // Change to your server's IP address
#define BUFFER_SIZE 1024

int sockfd;
pthread_t receive_thread;

void *receive_messages(void *arg) {
    char message[BUFFER_SIZE];
    while (1) {
        int length = recv(sockfd, message, BUFFER_SIZE, 0);
        if (length > 0) {
            printf("%s", message);
            memset(message, 0, BUFFER_SIZE);
        }
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    char username[10];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [server-port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int SERVER_PORT = atoi(argv[1]);

    printf("username: ");
    fgets(username, 10, stdin);
    username[strcspn(username, "\n")] = 0;

    // Creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if(send(sockfd, username, strlen(username),0)<0){
        perror("Error: Username sending failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server.\n");

    // Create thread for receiving messages
    if (pthread_create(&receive_thread, NULL, &receive_messages, NULL) != 0) {
        perror("Error: Thread creation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Sending messages
    char message[BUFFER_SIZE];
    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        if (send(sockfd, message, strlen(message), 0) < 0) {
            perror("Error: Message sending failed");
            break;
        }
    }

    close(sockfd);
    return 0;
}
