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

int main() {
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

    // Loop to continually prompt the user for checking file modifications or finding earliest departure
    char input[MAX_LINE_LENGTH];
    while (1) {
        printf("\nType 'check' to check if modified, 'find' to find the earliest departure, or 'exit' to exit: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\nError reading input. Exiting...\n");
            break;
        }

        if (strncmp(input, "check", 5) == 0) {
            check_and_update_timetable(&timetable, filename);
        } else if (strncmp(input, "find", 4) == 0) {
            char destination[61];
            char current_time[6];

            printf("Enter the destination station: ");
            if (fgets(destination, sizeof(destination), stdin) == NULL) {
                printf("\nError reading destination. Exiting...\n");
                break;
            }
            strtok(destination, "\n"); // Remove newline

            printf("Enter the current time (HH:MM): ");
            if (fgets(current_time, sizeof(current_time), stdin) == NULL) {
                printf("\nError reading current time. Exiting...\n");
                break;
            }
            strtok(current_time, "\n"); // Remove newline

            earliest_departure(&timetable, destination, current_time);
        } else if (strncmp(input, "exit", 4) == 0) {
            break;
        } else {
            printf("Invalid command.\n");
        }
    }

    // Clean up resources before exiting
    free(timetable.entries);

    return 0;
}

