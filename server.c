/*
Author: Philip Harlow
Class: cs425
Teacher: Homer
Program: This program will connect to a server and send requests for the server to send us a specified quote.
the user must input the command line as such "quote [port] [quotenumber] [host]" host is optional, it will default to localhost if not is put. 
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
int sock_desc, new_socket;
int port_number, readVal;

int main(int argc, char * argv[]) {

    struct sockaddr_in sin;
    int addrlen = sizeof(sin);
    int buflen, readVal, n, tempBuflen;
    char buffer[1026], *bufptr;
    bufptr = buffer;

    //create socket file descriptor
    if((sock_desc = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }

    //Check to see if the user inputed all the required arguments, if not terminate the program
    if(argc < 2) {
        perror("Invalid parameters, to run program please input as ./server [serverPort]\n");
        perror("\t[serverPort] is the port number you wish the server to watch for incoming connection\n");
        return 1;
    }

    //convert command line argument to int
    port_number = atoi(argv[1]);

    //set the protocol to PF_INET
    sin.sin_family = PF_INET;

    //set the port number to the specified port from command line
    sin.sin_port = htons(port_number);

    //set the socket to accept connections from any ip
    sin.sin_addr.s_addr = INADDR_ANY;

    //bind the server socket to the given port number
    if(bind(sock_desc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Server failed to bind port number\n");
        return 1;
    }

    if(listen(sock_desc, 1) < 0) {
        perror("Server failed listening for clients\n");
        return 1;
    }

    if((new_socket = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
        perror("Server failed on accept\n");
        return 1;
    }
    //first find the length of the payload
    while((readVal = read(new_socket, &buflen, 4)) > 0) {


        //convert the length to little-endian
        buflen = ntohl(buflen);
        tempBuflen = buflen;

        printf("%d\n", buflen);
        bufptr = buffer;
        
        while ( (tempBuflen > 0) && (n = read(new_socket, bufptr, tempBuflen)) > 0 ) {
            bufptr = bufptr + n;
            tempBuflen = tempBuflen - n;
        }
        printf("we exited the loop\n");
        //null terminate the string
        *bufptr = '\n';
        bufptr++;
        *bufptr = '\0';
        bufptr = buffer;

        fputs(bufptr, stdout);
        
    }

    return 0;

}
