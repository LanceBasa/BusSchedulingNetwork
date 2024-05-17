#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_LINE_LENGTH 256
#define MAX 80
#define SA struct sockaddr

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
} Timetable;

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

void read_timetable(const char *filename, Timetable *timetable) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int is_station_info_read = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue; // Skip comments

        if (!is_station_info_read) {
            sscanf(line, "%31[^,],%15[^,],%15s", timetable->stationName, timetable->longitude, timetable->latitude);
            is_station_info_read = 1;
            continue;
        }

        TimetableEntry entry;
        if (sscanf(line, "%5[^,],%15[^,],%60[^,],%5[^,],%60s",
                   entry.departureTime, entry.routeName, entry.departingFrom,
                   entry.arrivalTime, entry.arrivalStation) == 5) {
            add_timetable_entry(timetable, entry);
        }
    }

    fclose(file);
}

void print_timetable(const Timetable *timetable) {
    printf("Station: %s\n", timetable->stationName);
    printf("Location: %s, %s\n", timetable->longitude, timetable->latitude);
    for (size_t i = 0; i < timetable->numEntries; ++i) {
        printf("Departure: %s, Route: %s, From: %s, Arrival: %s, To: %s\n",
               timetable->entries[i].departureTime, timetable->entries[i].routeName,
               timetable->entries[i].departingFrom, timetable->entries[i].arrivalTime,
               timetable->entries[i].arrivalStation);
    }
}

void extract_station_name(const char *buffer, char *station_name) {
    const char *to_marker = "GET /?to=";
    const char *http_marker = " HTTP/1.1";
    
    const char *start_pos = strstr(buffer, to_marker);
    start_pos += strlen(to_marker);

    const char *end_pos = strstr(start_pos, http_marker);

    size_t station_len = end_pos - start_pos;
    strncpy(station_name, start_pos, station_len);
    station_name[station_len] = '\0'; // Null-terminate the station name
}

void send_to_neighbors(char **neighbors, int neighbor_count, const char *message) {
    struct sockaddr_in neighbor_addr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        return;
    }

    memset(&neighbor_addr, 0, sizeof(neighbor_addr));
    neighbor_addr.sin_family = AF_INET;

    // Loop through all neighbors
    for (int i = 0; i < neighbor_count; ++i) {
        char host[256];
        int port;

        // Extract host and port
        if (sscanf(neighbors[i], "%255[^:]:%d", host, &port) != 2) {
            fprintf(stderr, "Invalid neighbor address: %s\n", neighbors[i]);
            continue;
        }

        neighbor_addr.sin_port = htons(port);
        inet_pton(AF_INET, host, &neighbor_addr.sin_addr);

        // Send message to current neighbor
        sendto(sockfd, message, strlen(message), 0, (SA *)&neighbor_addr, sizeof(neighbor_addr));
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s station-name browser-port query-port neighbor-host:neighbor-port [additional-neighbor-host:neighbor-port ...]\n", argv[0]);
        return 1;
    }

    char timetable_filename[64];
    snprintf(timetable_filename, sizeof(timetable_filename), "tt-%s", argv[1]);

    printf("Loading timetable from file: %s\n", timetable_filename);
    Timetable timetable = {.entries = NULL, .numEntries = 0, .capacity = 10};
    timetable.entries = malloc(timetable.capacity * sizeof(TimetableEntry));
    if (!timetable.entries) {
        perror("Failed to allocate initial memory for timetable entries");
        return 1;
    }

    read_timetable(timetable_filename, &timetable);
    //print_timetable(&timetable);

    int browser_port = atoi(argv[2]);
    int query_port = atoi(argv[3]);
    int tcp_sock, udp_sock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);
    int maxfd;
    char buffer[MAX];

    // Neighbor addresses
    char **neighbors = &argv[4];
    int neighbor_count = argc - 4;

    // Setup TCP server
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == -1) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(browser_port);

    if (bind(tcp_sock, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("TCP socket bind failed");
        close(tcp_sock);
        exit(EXIT_FAILURE);
    }

    //TCP listening
    if (listen(tcp_sock, 5) != 0) {
        perror("TCP listen failed");
        close(tcp_sock);
        exit(EXIT_FAILURE);
    }

    // Setup UDP server
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        perror("UDP socket creation failed");
        close(tcp_sock);
        exit(EXIT_FAILURE);
    }

    servaddr.sin_port = htons(query_port);
    if (bind(udp_sock, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("UDP socket bind failed");
        close(tcp_sock);
        close(udp_sock);
        exit(EXIT_FAILURE);
    }

    // Prepare for select
    fd_set readfds;
    maxfd = tcp_sock > udp_sock ? tcp_sock : udp_sock;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sock, &readfds);
        FD_SET(udp_sock, &readfds);

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(tcp_sock, &readfds)) {
            int connfd = accept(tcp_sock, (SA *)&cliaddr, &cliaddr_len);
            if (connfd < 0) {
                perror("TCP accept failed");
                continue;
            }

            // Handle TCP client
            memset(buffer, 0, sizeof(buffer));
            
            if (read(connfd, buffer, sizeof(buffer)) > 0) {
                char station_name[256];
                extract_station_name(buffer, station_name);
                printf("Destination station name: %s\n", station_name);

                // Send the extracted station name to all neighbors
                send_to_neighbors(neighbors, neighbor_count, station_name);

                // Respond to the web
                char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, world!\n";
                write(connfd, response, strlen(response));
            }

            close(connfd);
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            int n = recvfrom(udp_sock, buffer, sizeof(buffer), 0, (SA *)&cliaddr, &cliaddr_len);
            if (n > 0) {
                buffer[n] = '\0';
                printf("%s\n", buffer);
            } else if (n < 0) {
                perror("UDP recvfrom failed");
            }
        }
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}

//./stationS2 BusportB 4003 4004 localhost:4006 localhost:4010 
//./stationS2 StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
// ./stationS2 JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 

//./stationS2 JunctionA 4001 4002 localhost:4010 localhost:4012 
//./stationS2 TerminalD 4007 4008 localhost:4006 
//./stationS2 BusportF 4011 4012 localhost:4002 