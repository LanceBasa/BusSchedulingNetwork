#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>  // Include this header for NI_MAXHOST

void get_ip_addresses() {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    // Retrieve the current interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Loop through the list of interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        // Check the family of the interface address
        int family = ifa->ifa_addr->sa_family;

        // We are interested in AF_INET (IPv4) and AF_INET6 (IPv6) addresses
        if (family == AF_INET || family == AF_INET6) {
            // Convert the address to a human-readable form
            int s = getnameinfo(ifa->ifa_addr,
                                (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                      sizeof(struct sockaddr_in6),
                                host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }

            printf("Interface: %s, Address: %s\n", ifa->ifa_name, host);
        }
    }

    // Free the interface address list
    freeifaddrs(ifaddr);
}

int main() {
    get_ip_addresses();
    return 0;
}
