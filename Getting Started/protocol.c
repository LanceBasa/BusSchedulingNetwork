#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Structure for storing neighbor information
typedef struct Neighbor {
    char station_name[256];
    int udp_port;
    struct Neighbor* next;
} Neighbor;

// Structure for sending pings
typedef struct {
    int udp_sock;
    char station_name[256];
    int source_port;
    char** neighbors;
    int num_neighbors;
} PingArgs;

Neighbor* head = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Add a neighbor to the list if not already present
void add_neighbor(const char* station_name, int udp_port) {
    pthread_mutex_lock(&list_mutex);
    Neighbor* current = head;
    while (current != NULL) {
        if (strcmp(current->station_name, station_name) == 0 && current->udp_port == udp_port) {
            pthread_mutex_unlock(&list_mutex);
            return; // Neighbor already exists
        }
        current = current->next;
    }

    Neighbor* new_neighbor = (Neighbor*)malloc(sizeof(Neighbor));
    strncpy(new_neighbor->station_name, station_name, sizeof(new_neighbor->station_name) - 1);
    new_neighbor->udp_port = udp_port;
    new_neighbor->next = head;
    head = new_neighbor;
    pthread_mutex_unlock(&list_mutex);
}

// Print neighboring stations
void print_neighbors() {
    pthread_mutex_lock(&list_mutex);
    const Neighbor* current = head;
    printf("List of neighboring stations:\n");
    while (current != NULL) {
        printf("Neighbor: %s at UDP port: %d\n", current->station_name, current->udp_port);
        current = current->next;
    }
    pthread_mutex_unlock(&list_mutex);
}

// Thread function to send pings periodically
void* send_pings_thread(void* args) {
    PingArgs* ping_args = (PingArgs*)args;
    for (int i = 0; i < 10; i++) { // Send pings multiple times
        for (int j = 0; j < ping_args->num_neighbors; j++) {
            char neighbor_host[256];
            int neighbor_port;
            sscanf(ping_args->neighbors[j], "%255[^:]:%d", neighbor_host, &neighbor_port);

            struct sockaddr_in dest_addr;
            char message[256];
            snprintf(message, sizeof(message), "%s:%d", ping_args->station_name, ping_args->source_port);

            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(neighbor_port);
            inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

            sendto(ping_args->udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            printf("UDP %d sending [%s:%d] to UDP %d\n", ping_args->source_port, ping_args->station_name, ping_args->source_port, neighbor_port);
        }
        sleep(2); // Interval between pings
    }
    return NULL;
}

// Thread function to listen for incoming pings
void* receive_pings_thread(void* args) {
    int udp_sock = *(int*)args;
    char buffer[256];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    while (1) {
        int len = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
        if (len > 0) {
            buffer[len] = '\0';
            printf("Received: '%s' from %s\n", buffer, inet_ntoa(sender_addr.sin_addr));
            char station_name[256];
            int sender_udp_port;
            sscanf(buffer, "%255[^:]:%d", station_name, &sender_udp_port);

            add_neighbor(station_name, sender_udp_port);
            print_neighbors();
        } else {
            perror("Error receiving data");
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s station-name browser-port query-port neighbor1 [neighbor2 ...]\n", argv[0]);
        return 1;
    }

    int udp_port = atoi(argv[3]);

    // Create UDP socket
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

    if (bind(udp_sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
        perror("UDP bind failed");
        close(udp_sock);
        return 1;
    }

    // Prepare ping arguments
    PingArgs ping_args = {
        .udp_sock = udp_sock,
        .source_port = udp_port,
        .neighbors = &argv[4],
        .num_neighbors = argc - 4
    };
    strncpy(ping_args.station_name, argv[1], sizeof(ping_args.station_name) - 1);

    // Create threads for sending and receiving pings
    pthread_t send_thread, receive_thread;
    pthread_create(&send_thread, NULL, send_pings_thread, &ping_args);
    pthread_create(&receive_thread, NULL, receive_pings_thread, &udp_sock);

    // Wait for threads to complete
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(udp_sock);
    return 0;
}

//./protocol BusportB 4003 4004 localhost:4006 localhost:4010 
//./protocol StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./protocol JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./protocol JunctionA 4001 4002 localhost:4010 localhost:4012 
//./protocol TerminalD 4007 4008 localhost:4006 
//./protocol BusportF 4011 4012 localhost:4002 