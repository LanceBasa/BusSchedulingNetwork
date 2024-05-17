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
    }
}

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

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcp_sock < 0 || udp_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(udp_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("UDP bind failed");
        close(udp_sock);
        return 1;
    }

    // Pinging neighbors
    for (int i = 4; i < argc; i++) {
        char neighbor_host[256];
        int neighbor_port;
        sscanf(argv[i], "%255[^:]:%d", neighbor_host, &neighbor_port);

        struct sockaddr_in dest_addr;
        char message[256];
        snprintf(message, sizeof(message), "%s:%d", station_name, udp_port);

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(neighbor_port);
        inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

        sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        printf("UDP %d sending [%s:%d] to UDP %d\n", udp_port, station_name, udp_port, neighbor_port);
    }
    printf("Timeout reached. No more pings will be sent.\n");

    // Print current list of neighbors
    print_neighbors();

    // Now enter the main select loop to handle TCP and UDP traffic
    fd_set readfds;
    struct timeval timeout;
    int maxfd = (tcp_sock > udp_sock ? tcp_sock : udp_sock) + 1;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sock, &readfds);
        FD_SET(udp_sock, &readfds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        if (select(maxfd, &readfds, NULL, NULL, &timeout) < 0) {
            perror("Select error");
            break;
        }


        current_time = time(NULL);
        if (current_time - last_ping >= PING_INTERVAL) {
            for (int i = 0; i < argc - 4; i++) {
                char neighbor_host[256];
                int neighbor_port;
                sscanf(neighbors_info[i], "%255[^:]:%d", neighbor_host, &neighbor_port);

                struct sockaddr_in dest_addr;
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(neighbor_port);
                inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

                char message[256];
                snprintf(message, sizeof(message), "%s:%d", station_name, udp_port);
                sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                printf("UDP %d sending [%s:%d] to UDP %d\n", udp_port, station_name, udp_port, neighbor_port);
            }
            last_ping = current_time;  // Reset the last ping time
        }

        if (FD_ISSET(tcp_sock, &readfds)) {
            int new_sock = accept(tcp_sock, NULL, NULL);
            if (new_sock < 0) {
                perror("Accept failed");
                continue;
            }
            char buffer[BUFFER_SIZE];
            read(new_sock, buffer, sizeof(buffer));
            printf("TCP received: %s\n", buffer);
            close(new_sock);
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            struct sockaddr_in cliaddr;
            socklen_t len = sizeof(cliaddr);
            char buffer[BUFFER_SIZE];
            int n = recvfrom(udp_sock, buffer, BUFFER_SIZE-1, 0, (struct sockaddr *)&cliaddr, &len);
            if (n > 0) {
                buffer[n] = '\0';
                printf("UDP received: %s from %s\n", buffer, inet_ntoa(cliaddr.sin_addr));
                char recv_station_name[256];
                int recv_port;
                sscanf(buffer, "%[^:]:%d", recv_station_name, &recv_port);
                add_neighbor(recv_station_name, recv_port);
                print_neighbors();
            }
        }
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}