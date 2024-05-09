/*
** showip.c -- show IP addresses for a host given on the command line
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char *argv[])
{
    int status;
    struct addrinfo hints, *res;
    int s;
    int sockfd;
    struct addrinfo *servinfo;  // will point to the results
    int yes=1;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    // if ((status = getaddrinfo(NULL, "4003", &hints, &servinfo)) != 0) {
    //     fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    //     exit(1);
    // }

    getaddrinfo("localhost", "4003", &hints, &res);

    //int socket(int domain, int type, int protocol);
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    //int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
    //bind it to the port we passed in to the getaddrinfo():
    bind(sockfd, res->ai_addr, res->ai_addrlen);

    // lose the pesky "Address already in use" error message
    // if (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
    //     perror("setsockopt");
    //     exit(1);
    // } 


    //connect(sockfd, res->ai_addr, res->ai_addrlen);

    listen(sockfd, 5);

    // servinfo now points to a linked list of 1 or more struct addrinfos

    // ... do everything until you don't need servinfo anymore ....

    freeaddrinfo(servinfo); // free the linked-list

	return 0;
}

