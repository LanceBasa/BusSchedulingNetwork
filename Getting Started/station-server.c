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

typedef struct {
    int port;
} ThreadArg;

void *tcp_server(void *arg) {
    ThreadArg *tcpArg = (ThreadArg *) arg;
    int port = tcpArg->port;

    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    socklen_t clilen = sizeof(cli);
    char buff[MAX];
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully created..\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        perror("socket bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully binded..\n");

    if (listen(sockfd, 5) != 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", port);

    connfd = accept(sockfd, (SA*)&cli, &clilen);
    if (connfd < 0) {
        perror("server accept failed");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        bzero(buff, MAX);
        read(connfd, buff, sizeof(buff));
        printf("From client: %s\n", buff);
    }

    close(sockfd);
    pthread_exit(NULL);
}


void *udp_server(void *arg) {
    ThreadArg *udpArg = (ThreadArg *) arg;
    int port = udpArg->port;

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[MAX];
    socklen_t len = sizeof(cliaddr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("UDP socket creation failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        perror("UDP socket bind failed");
        exit(1);
    }

    printf("UDP server listening on port %d...\n", port);
    recvfrom(sockfd, buffer, MAX, 0, (SA*)&cliaddr, &len);
    printf("Received UDP message: %s\n", buffer);

    close(sockfd);
    pthread_exit(NULL);
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

    pthread_t tcp_thread, udp_thread;
    ThreadArg tcpArgs, udpArgs;

    tcpArgs.port = atoi(argv[2]);
    udpArgs.port = atoi(argv[3]);

    pthread_create(&tcp_thread, NULL, tcp_server, &tcpArgs);
    pthread_create(&udp_thread, NULL, udp_server, &udpArgs);

    pthread_join(tcp_thread, NULL);
    pthread_join(udp_thread, NULL);

    return 0;
}
