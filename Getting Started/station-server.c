#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char departTime[6];
    char routeName[20];
    char departFrom[20];
    char arriveTime[6];
    char arriveAt[20];
} TimetableEntry;

typedef struct {
    char stationName[50];
    double longitude;
    double latitude;
    TimetableEntry *entries;
    int entryCount;
    int capacity;
} Station;

typedef struct {
    Station *stations;
    int stationCount;
    int capacity;
} StationArray;

void initStationArray(StationArray *array, int initialCapacity) {
    array->stations = malloc(sizeof(Station) * initialCapacity);
    array->stationCount = 0;
    array->capacity = initialCapacity;
}

void resizeStationArray(StationArray *array) {
    int newCapacity = array->capacity * 2;
    array->stations = realloc(array->stations, sizeof(Station) * newCapacity);
    array->capacity = newCapacity;
}

void addStation(StationArray *array, const char* name, double lon, double lat, int initialEntriesCapacity) {
    if (array->stationCount == array->capacity) {
        resizeStationArray(array);
    }
    Station *station = &array->stations[array->stationCount++];
    strcpy(station->stationName, name);
    station->longitude = lon;
    station->latitude = lat;
    station->entryCount = 0;
    station->capacity = initialEntriesCapacity;
    station->entries = malloc(sizeof(TimetableEntry) * initialEntriesCapacity);
}

void resizeTimetableEntries(Station *station) {
    int newCapacity = station->capacity * 2;
    station->entries = realloc(station->entries, sizeof(TimetableEntry) * newCapacity);
    station->capacity = newCapacity;
}

void addTimetableEntry(StationArray *array, const char* stationName, TimetableEntry entry) {
    for (int i = 0; i < array->stationCount; i++) {
        if (strcmp(array->stations[i].stationName, stationName) == 0) {
            Station *station = &array->stations[i];
            if (station->entryCount == station->capacity) {
                resizeTimetableEntries(station);
            }
            station->entries[station->entryCount++] = entry;
            return;
        }
    }
}

Station* findOrAddStation(StationArray *array, const char* name, double lon, double lat) {
    for (int i = 0; i < array->stationCount; i++) {
        if (strcmp(array->stations[i].stationName, name) == 0) {
            return &array->stations[i];
        }
    }
    addStation(array, name, lon, lat, 10);
    return &array->stations[array->stationCount - 1];
}

void freeStationArray(StationArray *array) {
    for (int i = 0; i < array->stationCount; i++) {
        free(array->stations[i].entries);
    }
    free(array->stations);
}

void printStations(const StationArray *array) {
    for (int i = 0; i < array->stationCount; i++) {
        printf("Station: %s, Location: (%.4f, %.4f)\n", array->stations[i].stationName, array->stations[i].longitude, array->stations[i].latitude);
        for (int j = 0; j < array->stations[i].entryCount; j++) {
            printf("\tDepart: %s, Route: %s, From: %s, Arrive: %s, At: %s\n",
                   array->stations[i].entries[j].departTime, array->stations[i].entries[j].routeName,
                   array->stations[i].entries[j].departFrom, array->stations[i].entries[j].arriveTime,
                   array->stations[i].entries[j].arriveAt);
        }
    }
}

void parseCSV(const char *filename, StationArray *stationArray) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open file");
        return;
    }

    char line[128];
    char stationName[50];
    double longitude, latitude;
    Station *currentStation = NULL;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') {
            continue;  // Skip comment lines
        } else if (strchr(line, ',')) {
            // Determine if it's a station or timetable line
            if (sscanf(line, "%49[^,],%lf,%lf", stationName, &longitude, &latitude) == 3) {
                // It's a station line
                currentStation = findOrAddStation(stationArray, stationName, longitude, latitude);
            } else {
                // It's a timetable line
                TimetableEntry entry;
                sscanf(line, "%5[^,],%19[^,],%19[^,],%5[^,],%19[^\n]",
                       entry.departTime, entry.routeName, entry.departFrom, entry.arriveTime, entry.arriveAt);
                if (currentStation != NULL) {
                    addTimetableEntry(stationArray, currentStation->stationName, entry);
                }
            }
        }
    }

    fclose(file);
}

int main() {
    StationArray stationArray;
    initStationArray(&stationArray, 2);  // Initial capacity

    const char *filename = "tt-BusportB";
    parseCSV(filename, &stationArray);

    printStations(&stationArray);

    freeStationArray(&stationArray);  // Free dynamically allocated memory

    return 0;
}
