// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 10

int main() {
    int server_fd, new_socket, clients[MAX_CLIENTS];
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    fd_set readfds;

    // Initialize clients array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = 0;
    }

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for an activity on one of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        // If something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // Inform user of socket number - used in send and receive commands
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Add new socket to array of clients
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == 0) {
                    clients[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Else, it's some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i];

            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread;
                if ((valread = read(sd, buffer, 1024)) == 0) {
                    // Somebody disconnected, get his details and print
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    clients[i] = 0;
                } else {
                    // Set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';

                    // Broadcast the message to all clients, including the sender
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j] != 0) {
                            send(clients[j], buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}