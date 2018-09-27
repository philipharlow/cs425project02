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
int sock_desc, sock_desc2, new_socket;  //new_socket will be the socket listeing to the localhost telnet daemon, sock_desc2 will be listing to the clientProxy
int port_number, readVal;
void readString(int readSock, int writeSock);

int main(int argc, char * argv[]) {
    fd_set listen;
    struct timeval timeout;
    int nfound;
    struct sockaddr_in sin, sout;
    int addrlen = sizeof(sin);
    int buflen, readVal;

    //create socket file descriptor
    if((sock_desc = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }
    if((new_socket = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }

    //Check to see if the user inputed all the required arguments, if not terminate the program
    if(argc < 2) {
        perror("Invalid parameters, to run program please input as server [serverPort]\n");
        perror("\t[serverPort] should always be 6200\n");
        return 1;
    }

    //convert command line argument to int
    port_number = atoi(argv[1]);

    //set the protocol to PF_INET
    sin.sin_family = PF_INET;
    sout.sin_family = PF_INET;

    //set the port number to the specified port from command line
    sin.sin_port = htons(port_number);
    sout.sin_port = htons(23);

    //set the socket to accept connections from any ip
    sin.sin_addr.s_addr = INADDR_ANY;

    //set our address to the telnet daemon
    in_addr_t address;
	address = inet_addr("localhost");
	if(address == -1){
		fprintf(stderr, "ERROR: Could not get address from IP.\n");
		exit(1);
	}
	memcpy(&sout.sin_addr, &address, sizeof(address));

    //bind the server socket to the given port number
    if(bind(sock_desc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Server failed to bind port number\n");
        return 1;
    }

    if(listen(sock_desc, 1) < 0) {
        perror("Server failed listening for clients\n");
        return 1;
    }

    if((sock_desc2 = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
        perror("Server failed on accept\n");
        return 1;
    }
    
    /* Connect to the server */
	int connectDesc = connect(new_socket, (struct sockaddr*)&sout, sizeof(sout));
	if(connectDesc < 0){
		fprintf(stderr, "ERROR: could not connect to telnet daemon\n");
		exit(1);
	}

    while(1) {
        FD_ZERO(&listen);
        FD_SET(new_socket, &listen);
        FD_SET(sock_desc2, &listen);
        timeout.tv_sec = 600;
        timeout.tv_usec = 0;

        nfound = select(sock_desc2 + 1, &listen, (fd_set *)0, (fd_set *)0, &timeout);

        if(nfound == 0) {
            //we timed out
            //close the sockets
            close(sock_desc);
            close(sock_desc2);
            close(new_socket);
            exit(0);
        }
        else if(nfound < 0) {
            //something went wrong. handle the error
        }
        else {
            if(FD_ISSET(new_socket, &listen)) {
                //if this is set we know we must read from the telnet daemon and pass it to the clientProxy
                readString(new_socket, sock_desc2);
            }
            if(FD_ISSET(sock_desc2, &listen)) {
                //if this is set we must read from the clientProxy and pass it on to the telnet daemon
                readString(sock_desc2, new_socket);
            }
        }
    }

    return 0;

}

//a function to read in the bytes from the socket and store them into a buffer and print that string to stdout
void readString(int readSock, int writeSock) {
    //initialize the buffer
    char buf[500], *ptr;
    ptr = buf;
    //read in the bytes
    int val = read(readSock, ptr, 500);
    if(val == 0){
		exit(0);
	}
    //null terminate the buffer
    ptr[buflen] = 0;
    //write to the destination socket
    int result = write(writeSock, ptr, strlen(ptr));
	if(result < 0){
		fprintf(stderr, "ERROR: Couldn't write size to server.\n");
		exit(1);
	}
}