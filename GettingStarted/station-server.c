#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <TCP port> <UDP port>\n", argv[0]);
        return 1;
    }

    int tcpPort = atoi(argv[1]);
    int udpPort = atoi(argv[2]);

    // Your server logic goes here
    // For demonstration purposes, this just prints the ports
    printf("TCP Port: %d\n", tcpPort);
    printf("UDP Port: %d\n", udpPort);

    // Simulating server activity
    sleep(10);  // Simulate some processing time

    return 0;
}