// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

void *receive_messages(void *socket) {
    int sock = *(int *)socket;
    char buffer[1024];
    int valread;
    while ((valread = read(sock, buffer, 1024)) > 0) {
        buffer[valread] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char message[1024];
    pthread_t recv_thread;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Create a thread to receive messages from the server
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sock);

    while (1) {
        printf("Enter message: ");
        fgets(message, 1024, stdin);
        send(sock, message, strlen(message), 0);
    }

    // Wait for the receive thread to finish
    pthread_join(recv_thread, NULL);

    return 0;
}