#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE_LENGTH 256

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

    // Update the last modified time
    timetable->lastModified = time(NULL);
}

void print_timetable(const Timetable *timetable) {
    printf("Station: %s\n", timetable->stationName);
    printf("Location: %s, %s\n", timetable->longitude, timetable->latitude);
    printf("Last modified: %s", ctime(&timetable->lastModified));
    for (size_t i = 0; i < timetable->numEntries; ++i) {
        printf("Departure: %s, Route: %s, From: %s, Arrival: %s, To: %s\n",
               timetable->entries[i].departureTime, timetable->entries[i].routeName,
               timetable->entries[i].departingFrom, timetable->entries[i].arrivalTime,
               timetable->entries[i].arrivalStation);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s timetable-file\n", argv[0]);
        return 1;
    }

    printf("Loading timetable from file: %s\n", argv[1]);
    Timetable timetable = {.entries = NULL, .numEntries = 0, .capacity = 10};
    timetable.entries = malloc(timetable.capacity * sizeof(TimetableEntry));
    if (!timetable.entries) {
        perror("Failed to allocate initial memory for timetable entries");
        return 1;
    }

    read_timetable(argv[1], &timetable);
    print_timetable(&timetable);

    char input[16];
    printf("\nDid you modify it? (type any value to reprint the last modified data): ");
    fgets(input, sizeof(input), stdin);
    printf("\nReprinting the last modified data:\n");
    print_timetable(&timetable);

    free(timetable.entries);
    return 0;
}
