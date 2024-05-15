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
#include <stddef.h>

#define MAX_LINE_LENGTH 256
#define MAX 80
#define SA struct sockaddr

#define BUFFER_SIZE 1024
#define MAX_NEIGHBORS 20
#define PING_DURATION 10 // Duration to send pings

#define MAX_FILENAME_LENGTH 61 // Adjust as needed

const char* ip_address = "10.0.0.24";

typedef struct {
    char station_name[256];
    char address[256];
    int udp_port;
} Neighbor;

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

Neighbor neighbors[MAX_NEIGHBORS];
int neighbor_count = 0;

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

    printf("Number of Entries: %zu\n", timetable->numEntries);
    printf("Entries Capacity: %zu\n", timetable->capacity);

    for (size_t i = 0; i < timetable->numEntries; ++i) {
        printf("Index: %zu\n", i);
        printf("Departure: %s, Route: %s, From: %s, Arrival: %s, To: %s\n",
               timetable->entries[i].departureTime, timetable->entries[i].routeName,
               timetable->entries[i].departingFrom, timetable->entries[i].arrivalTime,
               timetable->entries[i].arrivalStation);
    }

    printf("TIMETABLE HAS BEEN PRINTED\n");
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
    //print_timetable(timetable);
    printf("Finished CHECK AND UPDATE TIMETABLE FUNCTION\n");
}

void earliest_departure(const Timetable *timetable, const char *destination, const char *current_time, TimetableEntry *result_entry) {
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
        *result_entry = *earliest_entry;
    } else {
        result_entry->departureTime[0] = '\0'; // Indicate no available departure
    }
}

void add_neighbor(const char* station_name, int udp_port) {
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbors[i].udp_port == udp_port) {
            strcpy(neighbors[i].station_name, station_name);
        }
    }

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
    printf("\n=== Neighbor List ===\n");
    for (int i = 0; i < neighbor_count; i++) {
        printf("NEIGHBOR: %s:%d at Address: %s\n", neighbors[i].station_name, neighbors[i].udp_port, neighbors[i].address);
    }
    printf("=====================\n\n");
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
    addr.sin_addr.s_addr = inet_addr(ip_address);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("TCP bind failed");
        close(sock);
        exit(1);
    }

    listen(sock, 5);
    printf("TCP server listening for incoming connections on port %d \n", port);

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
    addr.sin_addr.s_addr = inet_addr(ip_address);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP bind failed");
        close(sock);
        exit(1);
    }
    printf("UDP server listening on port %d \n", port);

    return sock;
}

void send_initial_pings(int udp_sock, const char* station_name, int source_port, char** neighbors, int num_neighbors) {
    for (int j = 0; j < 7; j++) {
        for (int i = 0; i < num_neighbors; i++) {
            char neighbor_host[256];
            int neighbor_port;

            sscanf(neighbors[i], "%255[^:]:%d", neighbor_host, &neighbor_port);

            struct sockaddr_in dest_addr;
            char message[256];
            snprintf(message, sizeof(message), "!%s:%d", station_name, source_port);

            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(neighbor_port);
            inet_pton(AF_INET, neighbor_host, &dest_addr.sin_addr);

            sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        sleep(1);
    }
    sleep(PING_DURATION); // Wait for the duration to give neighbors time to respond
}

char* extract_station_name_from_http_request(char *http_request) {
    const char *to_keyword = "to=";
    char *start = strstr(http_request, to_keyword);
    if (!start) return NULL;

    start += strlen(to_keyword);
    char *end = strchr(start, ' ');
    if (!end) end = strchr(start, '\r'); // Check for end of line if space not found
    if (!end) return NULL;

    ptrdiff_t length = end - start;
    char *station_name = malloc(length + 1);
    if (!station_name) return NULL;

    strncpy(station_name, start, length);
    station_name[length] = '\0'; // Null-terminate the extracted string
    return station_name;
}

void cleanup(Timetable *timetable, int tcp_sock, int udp_sock) {
    free(timetable->entries);
    close(tcp_sock);
    close(udp_sock);
}

// Function to check if a station is in the path
int is_station_in_path(const char *station_name, const char *path) {
    char copy_path[BUFFER_SIZE];
    strcpy(copy_path, path);
    char *token = strtok(copy_path, ";");
    while (token != NULL) {
        if (strcmp(token, station_name) == 0) {
            return 1;
        }
        token = strtok(NULL, ";");
    }
    return 0;
}

// Function to flood the network with the query
void flood_network(const char* query, int udp_sock, const char* current_station) {
    for (int i = 0; i < neighbor_count; i++) {
        if (!is_station_in_path(neighbors[i].station_name, query)) {
            struct sockaddr_in dest_addr;
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "%s;%s", query, current_station);

            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(neighbors[i].udp_port);
            inet_pton(AF_INET, neighbors[i].address, &dest_addr.sin_addr);

            printf("\nSending flood message to neighbor %s at UDP port %d with message: %s\n", neighbors[i].station_name, neighbors[i].udp_port, message);
            sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
    }
}

// Function to backtrack the path to the source
void backtrack(const char* path, int udp_sock) {
    char backtrack_path[BUFFER_SIZE];
    strcpy(backtrack_path, path);

    char* split_path = strtok(backtrack_path, "-");
    char* previous_station = NULL;

    while (split_path != NULL) {
        previous_station = split_path;
        split_path = strtok(NULL, "-");
    }

    if (previous_station != NULL) {
        for (int i = 0; i < neighbor_count; i++) {
            if (strcmp(neighbors[i].station_name, previous_station) == 0) {
                struct sockaddr_in dest_addr;
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(neighbors[i].udp_port);
                inet_pton(AF_INET, neighbors[i].address, &dest_addr.sin_addr);

                printf("\nBacktracking message to neighbor %s at UDP port %d with message: %s\n", neighbors[i].station_name, neighbors[i].udp_port, path);
                sendto(udp_sock, path, strlen(path), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                break;
            }
        }
    }
}

// Function to display the path in the specified format
void display_path(const char* path) {
    char copy_path[BUFFER_SIZE];
    strcpy(copy_path, path);

    char* tokens = strtok(copy_path, ";");
    while (tokens != NULL) {
        if (tokens[0] == '~') {
            tokens = strtok(NULL, ";");
            continue;
        }

        const char* station = tokens;
        const char* busNumber = strtok(NULL, ";");
        const char* departTime = strtok(NULL, ";");
        const char* arrivalTime = strtok(NULL, ";");
        const char* arriveAt = strtok(NULL, ";");

        printf("From %s catch %s leaving at %s and arrive at %s at %s\n",
               station, busNumber, departTime, arriveAt, arrivalTime);

        tokens = strtok(NULL, ";");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) { // Ensure minimum arguments to prevent segfault
        fprintf(stderr, "Usage: %s station-name tcp-port udp-port neighbor1 [neighbor2 ...]\n", argv[0]);
        return 1;
    }
    // Extract station name from command line arguments
    char *station_name = argv[1];

    int tcp_port = atoi(argv[2]);
    int udp_port = atoi(argv[3]);

    int tcp_sock = setup_tcp_socket(tcp_port);
    int udp_sock = setup_udp_socket(udp_port);

    int numNeighbor = argc - 4;

    // populate the neighbor struct
    for (int i = 0; i < numNeighbor; i++) {
        char address[256];
        int udp_port;

        sscanf(argv[i + 4], "%255[^:]:%d", address, &udp_port);

        strcpy(neighbors[i].station_name, "");
        strcpy(neighbors[i].address, address);
        neighbors[i].udp_port = udp_port;

        printf("%s:%d:%s\n", neighbors[i].address, neighbors[i].udp_port, neighbors[i].station_name);
    }

    // Construct dynamic filename
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, MAX_FILENAME_LENGTH, "tt-%s", station_name);

    Timetable timetable = { .entries = NULL, .numEntries = 0, .capacity = 10, .lastModified = 0 };

    timetable.entries = malloc(timetable.capacity * sizeof(TimetableEntry));
    if (!timetable.entries) {
        perror("Failed to allocate memory for timetable entries");
        return 1;
    }

    if (!read_timetable(filename, &timetable)) {
        fprintf(stderr, "Initial read failed\n");
        cleanup(&timetable, tcp_sock, udp_sock);
        return 1;
    }

    struct stat statbuf;
    if (stat(filename, &statbuf) == -1) {
        perror("Failed to get file status");
        cleanup(&timetable, tcp_sock, udp_sock);
        return 1;
    }
    timetable.lastModified = statbuf.st_mtime;

    send_initial_pings(udp_sock, station_name, udp_port, argv + 4, argc - 4);

    printf("Timeout reached. No more pings will be sent.\n");
    if (neighbor_count == 0) {
        printf("No neighbors found.\n");
    } else {
        print_neighbors();
    }

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

            char *extracted_station_name = extract_station_name_from_http_request(buffer);
            if (extracted_station_name) {
                printf("Extracted station name for flooding: %s\n", extracted_station_name);

                check_and_update_timetable(&timetable, filename);

                for (int i = 0; i < neighbor_count; i++) {
                    TimetableEntry earliest_entry;
                    earliest_departure(&timetable, neighbors[i].station_name, "10:30", &earliest_entry);

                    if (earliest_entry.departureTime[0] != '\0') {
                        char message[BUFFER_SIZE];
                        snprintf(message, sizeof(message), "%s;%s;%s;%s;%s;%s",
                                extracted_station_name, station_name, earliest_entry.routeName,
                                earliest_entry.departureTime, earliest_entry.arrivalTime, earliest_entry.arrivalStation);

                        struct sockaddr_in dest_addr;
                        memset(&dest_addr, 0, sizeof(dest_addr));
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(neighbors[i].udp_port);
                        inet_pton(AF_INET, neighbors[i].address, &dest_addr.sin_addr);

                        printf("\nFlooding message to neighbor %s at UDP port %d with message: %s\n", neighbors[i].station_name, neighbors[i].udp_port, message);
                        sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                    }
                }

                free(extracted_station_name);
            } else {
                printf("Station name not found in HTTP request\n");
            }

            snprintf(buffer, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nRequest received.\n");
            send(new_sock, buffer, strlen(buffer), 0);
            close(new_sock);
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            struct sockaddr_in sender_addr;
            socklen_t sender_addr_len = sizeof(sender_addr);
            int len = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);

            if (len > 0) {
                buffer[len] = '\0';

                // Find the sender station name
                char sender_station[256];
                int sender_port = ntohs(sender_addr.sin_port);
                int sender_found = 0;

                for (int i = 0; i < neighbor_count; i++) {
                    if (neighbors[i].udp_port == sender_port && strcmp(inet_ntoa(sender_addr.sin_addr), neighbors[i].address) == 0) {
                        strcpy(sender_station, neighbors[i].station_name);
                        sender_found = 1;
                        break;
                    }
                }

                if (!sender_found) {
                    snprintf(sender_station, sizeof(sender_station), "unknown (%s:%d)", inet_ntoa(sender_addr.sin_addr), sender_port);
                }

                printf("\nReceived UDP message: '%s' from %s\n", buffer, sender_station);

                char dataIdentifyer = buffer[0];
                printf("Data identifier: '%c'\n", dataIdentifyer);

                if (dataIdentifyer == '!') {
                    char station_name[256];
                    int sender_udp_port;
                    sscanf(buffer, "!%255[^:]:%d", station_name, &sender_udp_port);
                    add_neighbor(station_name, sender_udp_port);
                } else if (dataIdentifyer == '~') {
                    display_path(buffer + 1);
                    backtrack(buffer + 1, udp_sock);
                } else {
                    printf("\nQuery\n");
                    check_and_update_timetable(&timetable, filename);

                    char path[BUFFER_SIZE];
                    strcpy(path, buffer);

                    char* tokens = strtok(buffer, ";");
                    char* final_destination = tokens;

                    char* last_token = NULL;
                    char* second_last_token = NULL;

                    while (tokens != NULL) {
                        second_last_token = last_token;
                        last_token = tokens;
                        tokens = strtok(NULL, ";");
                    }

                    char* current_time = second_last_token;

                    printf("Current time extracted from path: %s\n", current_time);

                    for (int i = 0; i < neighbor_count; i++) {
                        if (!is_station_in_path(neighbors[i].station_name, path)) {
                            TimetableEntry earliest_entry;
                            earliest_departure(&timetable, neighbors[i].station_name, current_time, &earliest_entry);

                            if (earliest_entry.departureTime[0] != '\0') {
                                char message[BUFFER_SIZE];
                                snprintf(message, sizeof(message), "%s;%s;%s;%s;%s;%s",
                                         path, station_name, earliest_entry.routeName,
                                         earliest_entry.departureTime, earliest_entry.arrivalTime, earliest_entry.arrivalStation);

                                struct sockaddr_in dest_addr;
                                memset(&dest_addr, 0, sizeof(dest_addr));
                                dest_addr.sin_family = AF_INET;
                                dest_addr.sin_port = htons(neighbors[i].udp_port);
                                inet_pton(AF_INET, neighbors[i].address, &dest_addr.sin_addr);

                                printf("\nSending flood message to neighbor %s at UDP port %d with message: %s\n", neighbors[i].station_name, neighbors[i].udp_port, message);
                                sendto(udp_sock, message, strlen(message), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                            } else {
                                printf("No more bus/train leaving at this hour to %s\n", neighbors[i].station_name);
                            }
                        }
                    }
                } 
            } else {
                perror("Error receiving UDP data");
            }
        }
    }

    cleanup(&timetable, tcp_sock, udp_sock);
    return 0;
}

//./final BusportB 4003 4004 localhost:4006 localhost:4010 
//./final StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./final JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./final JunctionA 4001 4002 localhost:4010 localhost:4012 
//./final TerminalD 4007 4008 localhost:4006 
//./final BusportF 4011 4012 localhost:4002 