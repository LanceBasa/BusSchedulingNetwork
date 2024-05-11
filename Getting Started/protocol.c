#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_LINE_LENGTH 256
#define BUFFER_SIZE 1024
#define PING_DURATION 10 // Time in seconds to send pings

typedef struct Neighbor {
    char stationName[256];
    int udpPort;
    struct Neighbor *next;
} Neighbor;

void send_ping(int udp_sock, const char* station_name, int source_port, const char* dest_host, int dest_port) {
    struct sockaddr_in dest_addr;
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s:%d", station_name, source_port);

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_host, &dest_addr.sin_addr);

    sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    printf("UDP %d sending [%s:%d] to UDP %d\n", source_port, station_name, source_port, dest_port);
}

// Function to add neighbor to the list if it does not already exist
void add_neighbor(Neighbor **head, char *stationName, int udpPort) {
    Neighbor *current = *head;
    while (current) {
        if (strcmp(current->stationName, stationName) == 0 && current->udpPort == udpPort) {
            return; // Neighbor already exists
        }
        current = current->next;
    }
    // Add new neighbor
    Neighbor *newNeighbor = malloc(sizeof(Neighbor));
    strcpy(newNeighbor->stationName, stationName);
    newNeighbor->udpPort = udpPort;
    newNeighbor->next = *head;
    *head = newNeighbor;
}

// Function to print the neighbors
void print_neighbors(Neighbor *head) {
    printf("List of neighboring stations:\n");
    while (head) {
        printf("Neighbor: %s at UDP port: %d\n", head->stationName, head->udpPort);
        head = head->next;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s station-name browser-port query-port neighbor1 [neighbor2 ...]\n", argv[0]);
        return 1;
    }

    int udp_port = atoi(argv[3]);
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("Failed to create UDP socket");
        return 1;
    }

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(udp_port);

    if (bind(udp_sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("UDP bind failed");
        close(udp_sock);
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = PING_DURATION;
    timeout.tv_usec = 0;

    // Send pings every second for PING_DURATION seconds
    for (int i = 0; i < PING_DURATION; i++) {
        for (int j = 4; j < argc; j++) {
            char neighbor_host[256];
            int neighbor_port;
            sscanf(argv[j], "%255[^:]:%d", neighbor_host, &neighbor_port);
            send_ping(udp_sock, argv[1], udp_port, neighbor_host, neighbor_port);
        }
        sleep(1);
    }

    // Set socket to non-blocking mode to wait for incoming pings
    setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    Neighbor *head = NULL; // Head of the list of neighbors
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
            printf("Waiting for pings...\n");
    while (1) {
        // Receive incoming pings
        int len = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
        if (len > 0) {
            buffer[len] = '\0';
            printf("Received: '%s' from %s\n", buffer, inet_ntoa(sender_addr.sin_addr));
            
            // Parse the received message to extract station name and UDP port
            char station_name[256];
            int udp_port;
            sscanf(buffer, "%255[^:]:%d", station_name, &udp_port);
            
            // Add the neighbor to the linked list if it does not exist already
            add_neighbor(&head, station_name, udp_port);
        } else {
            // If recvfrom returns -1 and errno is EAGAIN, it means the timeout has been reached
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout reached. No more pings will be received.\n");
                break;
            } else {
                perror("Failed to receive from UDP socket");
                break;
            }
        }
    }

    // Print all neighboring stations that were successfully received and stored
    print_neighbors(head);

    // Close the UDP socket
    close(udp_sock);

    // Free the memory allocated for the linked list
    Neighbor *current = head;
    while (current != NULL) {
        Neighbor *temp = current;
        current = current->next;
        free(temp);
    }

    return 0;
}

//./protocol BusportB 4003 4004 localhost:4006 localhost:4010 
//./protocol StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./protocol JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./protocol JunctionA 4001 4002 localhost:4010 localhost:4012 
//./protocol TerminalD 4007 4008 localhost:4006 
//./protocol BusportF 4011 4012 localhost:4002 