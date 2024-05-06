#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE       /* See feature_test_macros(7) */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_TIMETABLE_ENTRIES 100

struct timetable_entry {
    char departTime[6]; // Assuming time format HH:MM
    char routeName[20];
    char departFrom[20];
    char arriveTime[6]; // Assuming time format HH:MM
    char arriveAt[20];
};

struct timetable_entry timetable[MAX_TIMETABLE_ENTRIES];

void loadfile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[100];
    int index = 0;

    // Read header
    if (fgets(line, sizeof(line), file) != NULL) {
        // Assuming format: StationName,Longitude,Latitude
        char *token = strtok(line, ",");
        strcpy(timetable[index].departTime, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].routeName, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].departFrom, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].arriveTime, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].arriveAt, token);
        index++;
    }

    // Read timetable data
    while (fgets(line, sizeof(line), file) != NULL && index < MAX_TIMETABLE_ENTRIES) {
        // Assuming format: departTime,routeName,departFrom,arriveTime,arriveAt
        char *token = strtok(line, ",");
        strcpy(timetable[index].departTime, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].routeName, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].departFrom, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].arriveTime, token);
        token = strtok(NULL, ",");
        strcpy(timetable[index].arriveAt, token);
        index++;
    }

    fclose(file);
}

void earliest(const char *curTime) {
    struct tm tm_curTime;
    strptime(curTime, "%H:%M", &tm_curTime);

    printf("Current time is: %s\n", curTime);

    for (int i = 1; i < MAX_TIMETABLE_ENTRIES; i++) {
        struct tm tm_departTime;
        strptime(timetable[i].departTime, "%H:%M", &tm_departTime);
        if (difftime(mktime(&tm_departTime), mktime(&tm_curTime)) >= 0) {
            printf("Destination: %s\n", timetable[i].arriveAt);
            printf("Departure time: %s\n", timetable[i].departTime);
            break; // Assuming you only want to print the earliest destination
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <current time (HH:MM)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    const char *curTime = argv[2];

    loadfile(filename);
    earliest(curTime);

    return 0;
}
