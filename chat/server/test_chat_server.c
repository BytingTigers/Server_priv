#include "chat.h"


int main() {
    pthread_t thread1, thread2;
    int *port1 = malloc(sizeof(*port1));
    int *port2 = malloc(sizeof(*port2));

    if (!port1 || !port2) {
        perror("Failed to allocate memory for ports");
        return 1;
    }

    *port1 = 12345;
    *port2 = 12346;

    if (pthread_create(&thread1, NULL, start_chat_server, port1) != 0) {
        perror("Failed to create thread for server 12345");
        return 1;
    }

    if (pthread_create(&thread2, NULL, start_chat_server, port2) != 0) {
        perror("Failed to create thread for server 12346");
        return 1;
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    free(port1);
    free(port2);
    return 0;
}