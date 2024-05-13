#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024
#define MAX_NEIGHBORS 10

// Structure for storing neighbor information
typedef struct {
    char station_name[256];
    int udp_port;
} Neighbor;

Neighbor neighbors[MAX_NEIGHBORS];
int neighbor_count = 0;

// Add a neighbor to the list if not already present
void add_neighbor(const char* station_name, int udp_port) {
    for (int i = 0; i < neighbor_count; i++) {
        if (strcmp(neighbors[i].station_name, station_name) == 0 && neighbors[i].udp_port == udp_port) {
            return; // Neighbor already exists
        }
    }
    if (neighbor_count < MAX_NEIGHBORS) {
        strncpy(neighbors[neighbor_count].station_name, station_name, sizeof(neighbors[neighbor_count].station_name) - 1);
        neighbors[neighbor_count].udp_port = udp_port;
        neighbor_count++;
    }
}

// Print neighboring stations
void print_neighbors() {
    printf("List of neighboring stations:\n");
    for (int i = 0; i < neighbor_count; i++) {
        printf("Neighbor: %s at UDP port: %d\n", neighbors[i].station_name, neighbors[i].udp_port);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s station-name tcp-port udp-port neighbor1 [neighbor2 ...]\n", argv[0]);
        return 1;
    }

    char *station_name = argv[1];
    int tcp_port = atoi(argv[2]);
    int udp_port = atoi(argv[3]);
    char** neighbors_info = &argv[4];

    // Set up TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("Failed to create TCP socket");
        return 1;
    }

    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(tcp_port);

    if (bind(tcp_sock, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("TCP bind failed");
        close(tcp_sock);
        return 1;
    }

    listen(tcp_sock, 5);

    // Set up UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("Failed to create UDP socket");
        return 1;
    }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_addr.sin_port = htons(udp_port);

    if (bind(udp_sock, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("UDP bind failed");
        close(udp_sock);
        return 1;
    }

    fd_set readfds;
    struct timeval timeout;
    char buffer[BUFFER_SIZE];
    int maxfd = (tcp_sock > udp_sock ? tcp_sock : udp_sock) + 1;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sock, &readfds);
        FD_SET(udp_sock, &readfds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int activity = select(maxfd, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(tcp_sock, &readfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int new_sock = accept(tcp_sock, (struct sockaddr*)&cli_addr, &cli_len);
            if (new_sock < 0) {
                perror("Accept failed");
                continue;
            }

            read(new_sock, buffer, BUFFER_SIZE - 1);
            printf("Received TCP message: %s\n", buffer);
            snprintf(buffer, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nList of neighboring stations:\n");
            for (int i = 0; i < neighbor_count; i++) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "Neighbor: %s at UDP port: %d\n", neighbors[i].station_name, neighbors[i].udp_port);
            }
            send(new_sock, buffer, strlen(buffer), 0);
            close(new_sock);
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            struct sockaddr_in sender_addr;
            socklen_t sender_addr_len = sizeof(sender_addr);
            int len = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
            if (len > 0) {
                buffer[len] = '\0';
                printf("Received UDP message: '%s' from %s\n", buffer, inet_ntoa(sender_addr.sin_addr));
                char station_name[256];
                int sender_udp_port;
                sscanf(buffer, "%255[^:]:%d", station_name, &sender_udp_port);
                add_neighbor(station_name, sender_udp_port);
                print_neighbors();
            } else {
                perror("Error receiving UDP data");
            }
        }

        // Send periodic pings to neighbors
        static int ping_counter = 0;
        if (ping_counter++ % 10 == 0) {
            for (int i = 0; i < argc - 4; i++) {
                char neighbor_host[256];
                int neighbor_port;
                sscanf(neighbors_info[i], "%255[^:]:%d", neighbor_host, &neighbor_port);

                struct sockaddr_in dest_addr;
                char message[256];
                snprintf(message, sizeof(message), "%s:%d", station_name, udp_port);

                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(neighbor_port);
                inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

                sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                printf("UDP %d sending [%s:%d] to UDP %d\n", udp_port, station_name, udp_port, neighbor_port);
            }
        }
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}
