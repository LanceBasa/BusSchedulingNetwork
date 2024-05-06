
#include <fcntl.h>              // opening reading and writing to and from files
#include <sys/socket.h>         // for network sockets
#include <stdint.h>             // for uint_16 data type
#include <netinet/in.h>         // for in_address in struct sockaddr_in
#include <stdlib.h>             // for Error success or failure


int family, type, protocol;
int sd, socket(int, int, int);


struct sockaddr {
    uint16_t sa_family;        // address family 
    char    sa_data[108];     // up to 108 bytes of addr 
};

// Socket address, internet style.  
struct sockaddr_in {
    short   sin_family;       // AF_INET 
    unsigned short sin_port;         // 16-bit port number 
    struct  in_addr sin_addr; // 32-bit netid/hostid 
    char    sin_zero[8];      // unused 
};  

sd = socket(addr_family, type, protocol);


int write_file_to_server(int sd, const char filenm[])
{
//  ENSURE THAT WE CAN OPEN PROVIDED FILE
    int fd  = open(filenm, O_RDWR, 0);

    if(fd >= 0) {
        char  buffer[1024];
        int   nbytes;

//  COPY BYTES FROM FILE-DESCRIPTOR TO SOCKET-DESCRIPTOR
        while( (nbytes = read(fd, buffer, sizeof(buffer) ) )) {
            if(write(sd, buffer, nbytes) != nbytes) {
                close(fd);
                return 1;
            }
        }
        close(fd);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
//  ASK OUR OS KERNEL TO ALLOCATE RESOURCES FOR A SOCKET
    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sd < 0) {
        perror(argv[0]);     // issue a standard error message
        exit(EXIT_FAILURE);
    }

//  FIND AND CONNECT TO THE SERVICE ADVERTISED WITH "THREEDsocket"
    if(connect(sd,"THREEDsocket",strlen("THREEDsocket")) == -1) {    
        perror(argv[0]);     // issue a standard error message
        exit(EXIT_FAILURE);
    }
    write_file_to_server(sd, FILENM_OF_COMMANDS);
    shutdown(sd, SHUT_RDWR);
    close(sd);
    exit(EXIT_SUCCESS);
}