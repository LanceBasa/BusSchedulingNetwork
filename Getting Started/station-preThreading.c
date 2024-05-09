#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_LINE_LENGTH 256
#define MAX 80
#define SA struct sockaddr

typedef struct {
    char departureTime[6];
    char routeName[16];
    char departingFrom[16];
    char arrivalTime[6];
    char arrivalStation[16];
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
        if (sscanf(line, "%5[^,],%15[^,],%15[^,],%5[^,],%15s",
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

void tcp_server(int tcp_port) {
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("TCP socket creation failed...\n");
        exit(0);
    } else {
        printf("TCP Socket successfully created..\n");
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(tcp_port);

    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("TCP socket bind failed...\n");
        exit(0);
    } else {
        printf("TCP Socket successfully binded..\n");
    }

    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    } else {
        printf("TCP Server listening on port %d...\n", tcp_port);
    }

    len = sizeof(cli);
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) {
        printf("TCP server accept failed...\n");
        exit(0);
    } else {
        printf("TCP server accepted the client..\n");
    }

    char buff[MAX];
    int n;
    for (;;) {
        bzero(buff, MAX);
        read(connfd, buff, sizeof(buff));
        printf("From client: %s\t To client: ", buff);
        bzero(buff, MAX);
        n = 0;
        while ((buff[n++] = getchar()) != '\n');

        write(connfd, buff, sizeof(buff));
        if (strncmp("exit", buff, 4) == 0) {
            printf("TCP Server Exit...\n");
            break;
        }
    }

    close(sockfd);
}

void udp_server(int udp_port) {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[MAX_LINE_LENGTH];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("UDP socket creation failed...\n");
        exit(0);
    } else {
        printf("UDP Socket successfully created..\n");
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(udp_port);

    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("UDP socket bind failed...\n");
        exit(0);
    } else {
        printf("UDP Socket successfully binded on port %d...\n", udp_port);
    }

    printf("UDP Server listening on port %d...\n", udp_port);

    int len, n;
    for (;;) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (SA*)&cliaddr, &len);
        buffer[n] = '\0';
        printf("Client: %s\n", buffer);
        // Here you can process the received data, send response back, etc.
    }

    close(sockfd);
}


int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s station-name browser-port query-port [neighbour1 ...]\n", argv[0]);
        return 1;
    }

    char timetable_filename[64];
    snprintf(timetable_filename, sizeof(timetable_filename), "tt-%s", argv[1]);

    int browser_port = atoi(argv[2]);
    int query_port = atoi(argv[3]);

    printf("Loading timetable from file: %s\n", timetable_filename);
    Timetable timetable = {.entries = NULL, .numEntries = 0, .capacity = 10};
    timetable.entries = malloc(timetable.capacity * sizeof(TimetableEntry));
    if (!timetable.entries) {
        perror("Failed to allocate initial memory for timetable entries");
        return 1;
    }

    read_timetable(timetable_filename, &timetable);
    //print_timetable(&timetable);

    printf("Starting TCP server on port %d...\n", browser_port);
    tcp_server(browser_port);

    printf("Starting UDP server on port %d...\n", query_port);
    udp_server(query_port);

    free(timetable.entries);
    return 0;
}
