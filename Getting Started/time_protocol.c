#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>


//for timetable
#define MAX_LINE_LENGTH 256
#define MAX 80
#define SA struct sockaddr

#define BUFFER_SIZE 1024
#define MAX_NEIGHBORS 10
#define PING_DURATION 10 // Duration to send pings


#define MAX_LINE_LENGTH 256
#define MAX 80
#define SA struct sockaddr

typedef struct {
    char station_name[256];
    int udp_port;
} Neighbor;


//-------------------timetable stuff ----------------------


typedef struct {
    char departureTime[6];
    char routeName[16];
    char departingFrom[61];
    char arrivalTime[6];
    char arrivalStation[61];
} TimetableEntry;

typedef struct {
    char stationName[32];
    char longitude[16];
    char latitude[16];
    TimetableEntry *entries;
    size_t numEntries;
    size_t capacity;
    time_t lastModified;
} Timetable;


void add_timetable_entry(Timetable *timetable, TimetableEntry entry) {
    if (timetable->numEntries >= timetable->capacity) {
        timetable->capacity *= 2;
        TimetableEntry *new_entries = realloc(timetable->entries, timetable->capacity * sizeof(TimetableEntry));
        if (!new_entries) {
            perror("Failed to resize timetable entries");
            exit(1);
        }
        timetable->entries = new_entries;
    }
    timetable->entries[timetable->numEntries++] = entry;
}

int read_timetable(const char *filename, Timetable *timetable) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return 0; // Indicate failure
    }

    char line[MAX_LINE_LENGTH];
    int is_station_info_read = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue; // Skip comments
        TimetableEntry entry;
        if (!is_station_info_read) {
            if (sscanf(line, "%31[^,],%15[^,],%15s", timetable->stationName, timetable->longitude, timetable->latitude) == 3) {
                is_station_info_read = 1;
            }
            continue;
        }
        if (sscanf(line, "%5[^,],%15[^,],%60[^,],%5[^,],%60s", entry.departureTime, entry.routeName, entry.departingFrom,
                   entry.arrivalTime, entry.arrivalStation) == 5) {
            add_timetable_entry(timetable, entry);
        }
    }
    fclose(file);
    return 1; // Indicate success
}

void print_timetable(const Timetable *timetable) {
    if (timetable->entries == NULL) {
        printf("No timetable entries to display.\n");
        return;
    }
    printf("Station: %s\n", timetable->stationName);
    printf("Location: %s, %s\n", timetable->longitude, timetable->latitude);
    for (size_t i = 0; i < timetable->numEntries; ++i) {
        printf("Departure: %s, Route: %s, From: %s, Arrival: %s, To: %s\n",
               timetable->entries[i].departureTime, timetable->entries[i].routeName,
               timetable->entries[i].departingFrom, timetable->entries[i].arrivalTime,
               timetable->entries[i].arrivalStation);
    }
}

void check_and_update_timetable(Timetable *timetable, const char *filename) {
    struct stat statbuf;
    if (stat(filename, &statbuf) == -1) {
        perror("Failed to get file status");
        return;
    }

    if (difftime(statbuf.st_mtime, timetable->lastModified) != 0) {
        printf("FILE HAS BEEN MODIFIED\n");
        free(timetable->entries); // Free existing entries
        timetable->entries = NULL;
        timetable->numEntries = 0;
        timetable->capacity = 10; // Reset capacity
        timetable->entries = malloc(timetable->capacity * sizeof(TimetableEntry));
        if (!timetable->entries) {
            perror("Failed to allocate memory for timetable entries");
            return;
        }
        if (!read_timetable(filename, timetable)) {
            fprintf(stderr, "Failed to reload the timetable.\n");
            return;
        }
        timetable->lastModified = statbuf.st_mtime; // Update last modified time
    } else {
        printf("File has not been modified.\n");
    }
    print_timetable(timetable);
}

void earliest_departure(const Timetable *timetable, const char *destination, const char *current_time) {
    struct tm tm_current = {0}, tm_depart = {0};
    if (strptime(current_time, "%H:%M", &tm_current) == NULL) {
        fprintf(stderr, "Invalid current time format\n");
        return;
    }
    time_t current = mktime(&tm_current);
    time_t earliest_time = 0;
    const TimetableEntry *earliest_entry = NULL;

    for (size_t i = 0; i < timetable->numEntries; ++i) {
        if (strcmp(timetable->entries[i].arrivalStation, destination) == 0) {
            if (strptime(timetable->entries[i].departureTime, "%H:%M", &tm_depart) == NULL) {
                fprintf(stderr, "Invalid departure time format in timetable\n");
                continue;
            }
            time_t departure = mktime(&tm_depart);

            // Compare departure time with the given current time
            if (difftime(departure, current) >= 0 && (!earliest_entry || difftime(departure, earliest_time) < 0)) {
                earliest_time = departure;
                earliest_entry = &timetable->entries[i];
            }
        }
    }

    if (earliest_entry) {
        printf("Earliest departure to %s: %s via %s, arriving at %s\n",
               destination, earliest_entry->departureTime, earliest_entry->routeName, earliest_entry->arrivalTime);
    } else {
        printf("No available departure to %s after %s\n", destination, current_time);
    }
}


//----------------------------------------------------------------


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

/**
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
**/
int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s station-name tcp-port udp-port neighbor1 [neighbor2 ...]\n", argv[0]);
        return 1;
    }


//-----------------------------timetable main added------------
    char *filename = "tt-BusportB";
    Timetable timetable = { .entries = NULL, .numEntries = 0, .capacity = 10, .lastModified = 0 };

    // Allocate initial memory for timetable entries
    timetable.entries = malloc(timetable.capacity * sizeof(TimetableEntry));
    if (!timetable.entries) {
        perror("Failed to allocate memory for timetable entries");
        return 1;
    }

    // Initial read of the timetable
    if (!read_timetable(filename, &timetable)) {
        fprintf(stderr, "Initial read failed\n");
        return 1;
    }

    // Get the last modified time of the file
    struct stat statbuf;
    if (stat(filename, &statbuf) == -1) {
        perror("Failed to get file status");
        free(timetable.entries);
        return 1;
    }
    timetable.lastModified = statbuf.st_mtime;

    // Print the timetable initially
    print_timetable(&timetable);

    
//----------------------------------done...........................

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
    //handle_network_traffic(tcp_sock, udp_sock);
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


    close(tcp_sock);
    close(udp_sock);
    return 0;
}


//./tp BusportB 4003 4004 localhost:4006 localhost:4010 
//./tp StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./tp JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./tp JunctionA 4001 4002 localhost:4010 localhost:4012 
//./tp TerminalD 4007 4008 localhost:4006 
//./tp BusportF 4011 4012 localhost:4002 