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
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    char username[32];
} client_t;

client_t clients[MAX_CLIENTS];

// Function to get the current timestamp
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", t);
}

void broadcast_message(char *message, int sender_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
}

void send_private_message(char *message, char *recipient_username, int sender_socket) {
    int found_recipient = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0 && strcmp(clients[i].username, recipient_username) == 0) {
            send(clients[i].socket, message, strlen(message), 0);
            found_recipient = 1;
            break;
        }
    }
    if (!found_recipient) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "User '%s' is not found or is offline.\n", recipient_username);
        send(sender_socket, error_msg, strlen(error_msg), 0);
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    fd_set readfds;

    // Initialize clients array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = 0;
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
            int sd = clients[i].socket;
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
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Else, it's some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;

            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread;
                if ((valread = read(sd, buffer, 1024)) == 0) {
                    // Somebody disconnected, get his details and print
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    clients[i].socket = 0;
                    memset(clients[i].username, 0, sizeof(clients[i].username));
                } else {
                    // Set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';

                    // First message from the client is their username
                    if (strlen(clients[i].username) == 0) {
                        strncpy(clients[i].username, buffer, sizeof(clients[i].username) - 1);
                        printf("User %s connected.\n", clients[i].username);
                        char welcome_msg[1024];
                        snprintf(welcome_msg, sizeof(welcome_msg), "%s has joined the chat\n", clients[i].username);
                        send(sd, welcome_msg, strlen(welcome_msg), 0);
                    } else {
                        // Check if the message is a private message
                        if (buffer[0] == '/' && buffer[1] == 'p' && buffer[2] == 'm' && buffer[3] == ' ') {
                            // Parse recipient username and message
                            char *recipient_username = strtok(buffer + 4, " ");
                            char *message = strtok(NULL, "");

                            if (recipient_username && message) {
                                // Construct the private message
                                char timestamp[32];
                                get_timestamp(timestamp, sizeof(timestamp));
                                char pm_message[1100]; // Increased buffer size to accommodate timestamp, sender's username, and message
                                snprintf(pm_message, sizeof(pm_message), "%s [Private] %s: %s\n", timestamp, clients[i].username, message);
                                send_private_message(pm_message, recipient_username, sd);
                            } else {
                                // Invalid private message format
                                char error_msg[256];
                                snprintf(error_msg, sizeof(error_msg), "Invalid private message format. Use: /pm username message\n");
                                send(sd, error_msg, strlen(error_msg), 0);
                            }
                        } else {
                            // Broadcast the message to all clients with timestamp and username
                            char timestamp[32];
                            get_timestamp(timestamp, sizeof(timestamp));
                            char message[1100]; // Increased buffer size to accommodate timestamp, username, and message
                            snprintf(message, sizeof(message), "%s %s: %s\n", timestamp, clients[i].username, buffer);
                            printf("%s", message); // Print message to server console
                            broadcast_message(message, sd);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
