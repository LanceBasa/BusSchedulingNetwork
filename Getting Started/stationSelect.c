#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE_LENGTH 256
#define MAX 1024
#define SA struct sockaddr
#define TIMEOUT_SECONDS 12
#define RETRY_INTERVAL_SECONDS 1
#define MAX_RETRIES 10

time_t last_modified = 0;

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
    time_t lastModified;  // Add this field to track file modification time
} Timetable;

typedef struct Neighbor {
    char stationName[32];
    int udpPort;
    struct Neighbor *next;
} Neighbor;

typedef struct {
    Neighbor *head;
    int count;  // total count of neighbors added
} NeighborList;

void send_ping_to_neighbors(int udp_sock, char **neighbors, int neighbor_count, const char *myStationName, int myUdpPort) {
    struct sockaddr_in neighbor_addr;
    char message[256];
    snprintf(message, sizeof(message), "%s:%d", myStationName, myUdpPort);

    for (int i = 0; i < neighbor_count; ++i) {
        memset(&neighbor_addr, 0, sizeof(neighbor_addr));
        neighbor_addr.sin_family = AF_INET;

        char host[256];
        int port;
        sscanf(neighbors[i], "%255[^:]:%d", host, &port);
        inet_pton(AF_INET, host, &neighbor_addr.sin_addr);
        neighbor_addr.sin_port = htons(port);

        sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr));
    }
}

void add_neighbor(NeighborList *list, const char *stationName, int udpPort) {
    Neighbor *newNeighbor = malloc(sizeof(Neighbor));
    strcpy(newNeighbor->stationName, stationName);
    newNeighbor->udpPort = udpPort;
    newNeighbor->next = list->head;
    list->head = newNeighbor;
    list->count++;
}

void handle_incoming_pings(int udp_sock, NeighborList *list) {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    char buffer[256];

    int n = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
    if (n > 0) {
        buffer[n] = '\0';
        char stationName[32];
        int port;
        sscanf(buffer, "%31[^:]:%d", stationName, &port);

        // Avoid adding duplicates
        Neighbor *current = list->head;
        while (current != NULL) {
            if (strcmp(current->stationName, stationName) == 0) {
                return; // Already added
            }
            current = current->next;
        }

        // Add to neighbor list
        add_neighbor(list, stationName, port);
    }
}

void print_neighbors(const NeighborList *list) {
    printf("List of neighboring stations:\n");
    Neighbor *current = list->head;
    while (current != NULL) {
        printf("Station: %s, UDP Port: %d\n", current->stationName, current->udpPort);
        current = current->next;
    }
}


//------------------- TIMETABLE FUNCTIONS ------------------------------

void add_timetable_entry(Timetable *timetable, TimetableEntry entry) {
    if (timetable->numEntries >= timetable->capacity) {
        timetable->capacity *= 2;
        timetable->entries = realloc(timetable->entries, timetable->capacity * sizeof(TimetableEntry));
        if (!timetable->entries) {
            perror("Failed to resize timetable entries");
            exit(1);
        }
    }
    timetable->entries[timetable->numEntries++] = entry;
}

int read_timetable(const char *filename, Timetable *timetable) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return 0;  // Indicate failure
    }

    char line[MAX_LINE_LENGTH];
    int is_station_info_read = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue;
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
    return 1;  // Indicate success
}

void check_and_reload_timetable(Timetable *timetable, const char *filename) {
    struct stat statbuf;
    if (stat(filename, &statbuf) != -1) {
        if (timetable->lastModified != statbuf.st_mtime) {
            printf("Timetable file has been modified. Reloading...\n");
            free(timetable->entries);
            timetable->entries = NULL;
            timetable->numEntries = 0;
            timetable->capacity = 10;
            timetable->entries = malloc(timetable->capacity * sizeof(TimetableEntry));
            if (read_timetable(filename, timetable)) {
                timetable->lastModified = statbuf.st_mtime;  // Update modification time
            }
        }
    } else {
        perror("Failed to get file status");
    }
}


void print_timetable(const Timetable *timetable) {
    printf("Station: %s\n", timetable->stationName);
    printf("Location: %s, %s\n", timetable->longitude, timetable->latitude);
    for (size_t i = 0; i < timetable->numEntries; ++i) {
        printf("Departure: %s, Route: %s, From: %s, Arrival: %s, To: %s\n", timetable->entries[i].departureTime,
               timetable->entries[i].routeName, timetable->entries[i].departingFrom, timetable->entries[i].arrivalTime,
               timetable->entries[i].arrivalStation);
    }
}

int compare_time(const char *time1, const char *time2) {
    struct tm tm1, tm2;
    strptime(time1, "%H:%M", &tm1);
    strptime(time2, "%H:%M", &tm2);

    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    if (t1 < t2) return -1;
    if (t1 > t2) return 1;
    return 0;
}

TimetableEntry* earliest_transport(Timetable *timetable, const char *destination_station, const char *time) {
    TimetableEntry *earliest = NULL;
    for (size_t i = 0; i < timetable->numEntries; i++) {
        if (strcmp(timetable->entries[i].arrivalStation, destination_station) == 0 && compare_time(timetable->entries[i].departureTime, time) > 0) {
            if (earliest == NULL || compare_time(timetable->entries[i].departureTime, earliest->departureTime) < 0) {
                earliest = &timetable->entries[i];
            }
        }
    }
    return earliest;
}


// --------------------TIMETABLE FUNCTION ENDS ---------------------------

void extract_station_name(const char *buffer, char *station_name) {
    const char *to_marker = "GET /?to=";
    const char *http_marker = " HTTP/1.1";

    const char *start_pos = strstr(buffer, to_marker);
    start_pos += strlen(to_marker);

    const char *end_pos = strstr(start_pos, http_marker);

    size_t station_len = end_pos - start_pos;
    strncpy(station_name, start_pos, station_len);
    station_name[station_len] = '\0';
}

int wait_for_neighbors(int udp_sock, NeighborList *list, int expected_count, int timeout_seconds) {
    fd_set readfds;
    struct timeval timeout;
    int remaining_count = expected_count;

    while (remaining_count > 0) {
        FD_ZERO(&readfds);
        FD_SET(udp_sock, &readfds);

        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;

        int activity = select(udp_sock + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("Select error while waiting for neighbors");
            break;
        }

        if (activity == 0) {
            printf("Timeout reached while waiting for neighbors.\n");
            break;
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            handle_incoming_pings(udp_sock, list);
            if (list->count >= expected_count) {
                break;
            }
        }
    }

    return list->count;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s station-name browser-port query-port neighbor-host:neighbor-port [additional-neighbor-host:neighbor-port ...]\n", argv[0]);
        return 1;
    }

    char timetable_filename[64];
    snprintf(timetable_filename, sizeof(timetable_filename), "tt-%s", argv[1]);

    // CHECK: how many possible entries
    Timetable timetable = {
        .entries = malloc(10 * sizeof(TimetableEntry)),
        .numEntries = 0,
        .capacity = 10,
        .lastModified = 0
    };
    if (!read_timetable(timetable_filename, &timetable)) {
        fprintf(stderr, "Error: Could not read timetable from %s\n", timetable_filename);
        free(timetable.entries);
        exit(1);
    }

    struct stat statbuf;
    if (stat(timetable_filename, &statbuf) != -1) {
        timetable.lastModified = statbuf.st_mtime;  // Set the initial last modified time
    } else {
        perror("Failed to stat the initial timetable file");
    }


    last_modified = time(NULL);

    NeighborList neighborList = { .head = NULL, .count = 0 };

    int tcp_port = atoi(argv[2]);
    int udp_port = atoi(argv[3]);
    int num_neighbors = argc - 4;
    char **neighbors = argv + 4;

    // Create and bind TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == -1) {
        perror("Failed to create TCP socket");
        exit(1);
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(tcp_port);

    if (bind(tcp_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("TCP bind failed");
        close(tcp_sock);
        exit(1);
    }

    if (listen(tcp_sock, 5) != 0) {
        perror("TCP listen failed");
        close(tcp_sock);
        exit(1);
    }

    // Create and bind UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        perror("Failed to create UDP socket");
        close(tcp_sock);
        exit(1);
    }

    struct sockaddr_in udpaddr = {0};
    udpaddr.sin_family = AF_INET;
    udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpaddr.sin_port = htons(udp_port);

    if (bind(udp_sock, (struct sockaddr *)&udpaddr, sizeof(udpaddr)) != 0) {
        perror("UDP bind failed");
        close(tcp_sock);
        close(udp_sock);
        exit(1);
    }

    // Retry pinging neighbors up to a maximum number of attempts
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        send_ping_to_neighbors(udp_sock, neighbors, num_neighbors, argv[1], udp_port);
        sleep(RETRY_INTERVAL_SECONDS);
    }

    printf("Waiting for pings from neighbors...\n");
    wait_for_neighbors(udp_sock, &neighborList, num_neighbors, TIMEOUT_SECONDS);
    print_neighbors(&neighborList);

    while (1) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int client_fd = accept(tcp_sock, (struct sockaddr *)&cliaddr, &clilen);
        if (client_fd < 0) {
            perror("Server accept failed");
            continue;
        }

        char buffer[MAX] = {0};
        read(client_fd, buffer, sizeof(buffer));

        char destination_station[32];
        extract_station_name(buffer, destination_station);

        snprintf(buffer, sizeof(buffer),
                     "HTTP/1.1 200 OK\nContent-Type: text/html\n\n"
                     "<html><body>"
                     "<h2>Next Transport to %s</h2>"
                     //"<p>Departure: %s</p>"                    
                     "</body></html>",
                     destination_station);

        // TimetableEntry *entry = earliest_transport(&timetable, destination_station, "00:00");
        // if (entry) {
        //     snprintf(buffer, sizeof(buffer),
        //              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n"
        //              "<html><body>"
        //              "<h2>Next Transport to %s</h2>"
        //              "<p>Departure: %s</p>"
        //              "<p>Route: %s</p>"
        //              "<p>From: %s</p>"
        //              "<p>Arrival: %s</p>"
        //              "</body></html>",
        //              destination_station, entry->departureTime, entry->routeName, entry->departingFrom, entry->arrivalTime);
        // } else {
        //     snprintf(buffer, sizeof(buffer),
        //              "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n"
        //              "<html><body><h2>No transport found to %s</h2></body></html>", destination_station);
        // }

        write(client_fd, buffer, strlen(buffer));
        close(client_fd);

        //check_and_reload_timetable(&timetable, timetable_filename);
    }

    return 0;
}


//./stationS BusportB 4003 4004 localhost:4006 localhost:4010 
//./stationS StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./stationS JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./stationS JunctionA 4001 4002 localhost:4010 localhost:4012 
//./stationS TerminalD 4007 4008 localhost:4006 
//./stationS BusportF 4011 4012 localhost:4002 