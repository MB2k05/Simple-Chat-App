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
#define USER_FILE "users.txt"

typedef struct {
    int socket;
    char username[32];
} client_t;

client_t clients[MAX_CLIENTS];

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

int register_user(const char *username, const char *password) {
    FILE *fp = fopen(USER_FILE, "a+");
    if (fp == NULL) {
        perror("Error opening user file");
        return -1;
    }

    // Check if username already exists
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char *token = strtok(line, " ");
        if (token != NULL && strcmp(token, username) == 0) {
            fclose(fp);
            return -1; // User already exists
        }
    }

    // Add new user
    fprintf(fp, "%s %s\n", username, password);
    fclose(fp);
    return 0; // Registration successful
}

int authenticate_user(const char *username, const char *password) {
    FILE *fp = fopen(USER_FILE, "r");
    if (fp == NULL) {
        perror("Error opening user file");
        return -1;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char stored_username[32];
        char stored_password[32];
        if (sscanf(line, "%s %s", stored_username, stored_password) == 2) {
            if (strcmp(stored_username, username) == 0 && strcmp(stored_password, password) == 0) {
                fclose(fp);
                return 0; // Authentication successful
            }
        }
    }

    fclose(fp);
    return -1; // Authentication failed
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

                    // Check if the message is a registration request
                    if (buffer[0] == '/' && buffer[1] == 'r' && buffer[2] == 'e' && buffer[3] == 'g' && buffer[4] == ' ') {
                        // Parse username and password
                        char *username = strtok(buffer + 5, " ");
                        char *password = strtok(NULL, "");

                        if (username && password) {
                            int reg_result = register_user(username, password);
                            if (reg_result == 0) {
                                char success_msg[] = "Registration successful. You can now log in.\n";
                                send(sd, success_msg, sizeof(success_msg), 0);
                            } else if (reg_result == -1) {
                                // Registration failed (user already exists)
                                char error_msg[] = "Username already exists. Please choose another username.\n";
                                send(sd, error_msg, sizeof(error_msg), 0);
                            }
                        } else {
                            // Invalid registration format
                            char error_msg[] = "Invalid input. Format: /reg username password\n";
                            send(sd, error_msg, sizeof(error_msg), 0);
                        }
                    } else {
                        // Assume it's a login attempt (username password format)
                        // Parse username and password
                        char *username = strtok(buffer, " ");
                        char *password = strtok(NULL, "");

                        if (username && password) {
                            int auth_result = authenticate_user(username, password);
                            if (auth_result == 0) {
                                // Authentication successful
                                snprintf(buffer, sizeof(buffer), "Welcome, %s!\n", username);
                                send(sd, buffer, strlen(buffer), 0);
                                strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
                            } else {
                                // Authentication failed
                                char error_msg[] = "Invalid username or password.\n";
                                send(sd, error_msg, sizeof(error_msg), 0);
                            }
                        } else {
                            // Invalid input format for login attempt
                            char error_msg[] = "Invalid input. Format: username password\n";
                            send(sd, error_msg, sizeof(error_msg), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
