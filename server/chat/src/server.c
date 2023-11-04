#include <stdio.h>
#include <stdlib.h>

#include "chat.h"

#define BUFF_LEN 1024

void display_help() {
    fprintf(stdout, "Usage: start [port]\n");
}

int main(int argc, char* argv[]) {

    if(argc != 2) {
        display_help();
    }

    int port = atoi(argv[1]);

    if(port == 0) {
        char err_msg[BUFF_LEN];
        snprintf(err_msg, sizeof(err_msg), "Cound not convert to a port number: %s", argv[1]);
        perror(err_msg);
    }

    if(port < 1 || port > 65535) {
        printf("Invalid port: %d\n", port);
    }

    start_chat_server(&port);
    return 0;
}
