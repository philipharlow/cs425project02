/*
Author: Philip Harlow & Marcos Samayoa
Class: cs425
Teacher: Homer
Program:  This is a server, it opens a port to listen at and accepts one connection at a time from any ip_address,
It recieves any text sent from its client and prints that text to stdout. The server closes when the client terminates the connection.
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
int readString(int buflen);

int main(int argc, char * argv[]) {

    struct sockaddr_in sin;
    int addrlen = sizeof(sin);
    int buflen, readVal;

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
    // //first find the length of the payload, and exit while loop when EOF is recieved from client
    while((readVal = read(new_socket, &buflen, 4)) > 0) {

        //convert the length to little-endian
        buflen = ntohl(buflen);
        //print out the length
        printf("%d\n", buflen);
        //read in the actual payload from client
        readVal = readString(buflen);
        
    }

    return 0;

}

//a function to read in the bytes from the socket and store them into a buffer and print that string to stdout
int readString(int buflen) {
    //initialize the buffer
    char buf[buflen], *ptr;
    ptr = buf;
    //read in the bytes
    int val = read(new_socket, ptr, buflen);
    //null terminate the buffer
    ptr[buflen] = 0;
    //print the buffer
    printf("%s\n", ptr);
    return val;
}