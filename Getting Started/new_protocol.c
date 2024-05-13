#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024
#define MAX_NEIGHBORS 10
#define PING_DURATION 10 // Duration to send pings

typedef struct {
    char station_name[256];
    int udp_port;
} Neighbor;

Neighbor neighbors[MAX_NEIGHBORS];
int neighbor_count = 0;

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
        print_neighbors(); // Print after adding a new neighbor
    }
}

void print_neighbors() {
    printf("List of neighboring stations:\n");
    for (int i = 0; i < neighbor_count; i++) {
        printf("Neighbor: %s at UDP port: %d\n", neighbors[i].station_name, neighbors[i].udp_port);
    }
}

int setup_tcp_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create TCP socket");
        exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("TCP bind failed");
        close(sock);
        exit(1);
    }

    listen(sock, 5);
    return sock;
}

int setup_udp_socket(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Failed to create UDP socket");
        exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP bind failed");
        close(sock);
        exit(1);
    }

    return sock;
}

void send_initial_pings(int udp_sock, const char* station_name, int source_port, char** neighbors, int num_neighbors) {
    
    
    for(int j =0 ; j <7; j++){
        for (int i = 0; i < num_neighbors; i++) {
            char neighbor_host[256];
            int neighbor_port;
            sscanf(neighbors[i], "%255[^:]:%d", neighbor_host, &neighbor_port);

            struct sockaddr_in dest_addr;
            char message[256];
            snprintf(message, sizeof(message), "%s:%d", station_name, source_port);

            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(neighbor_port);
            inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

            sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            printf("UDP %d sending [%s:%d] to UDP %d\n", source_port, station_name, source_port, neighbor_port);
            
        }
        sleep(1);
    
    }
    sleep(PING_DURATION); // Wait for the duration to give neighbors time to respond
}

void handle_network_traffic(int tcp_sock, int udp_sock) {
    fd_set readfds;
    char buffer[BUFFER_SIZE];
    int maxfd = (tcp_sock > udp_sock ? tcp_sock : udp_sock) + 1;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sock, &readfds);
        FD_SET(udp_sock, &readfds);

        int activity = select(maxfd, &readfds, NULL, NULL, NULL);

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
            } else {
                perror("Error receiving UDP data");
            }
        }
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

    int tcp_sock = setup_tcp_socket(tcp_port);
    int udp_sock = setup_udp_socket(udp_port);

    send_initial_pings(udp_sock, station_name, udp_port, argv + 4, argc - 4);

    printf("Timeout reached. No more pings will be sent.\n");
    if (neighbor_count == 0) {
        printf("No neighbors found.\n");
    } else {
        print_neighbors();
    }

    // Main select loop to handle TCP connections and incoming UDP messages
    handle_network_traffic(tcp_sock, udp_sock);

    close(tcp_sock);
    close(udp_sock);
    return 0;
}
