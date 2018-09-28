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
int sock_desc, clientProxy, telnet;  //telnet will be the socket listening to the localhost telnet daemon, clientProxy will be listing to the clientProxy
int port_number, readVal;
void readString(int readSock, int writeSock);

int main(int argc, char * argv[]) {
	perror("we started running\n");
    fd_set listen1;
    struct timeval timeout;
    int nfound;
    struct sockaddr_in sin, sout;
    int addrlen = sizeof(sin);
    int buflen, readVal;
	int largestSocket;

    //create socket file descriptor
    if((sock_desc = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }
    if((telnet = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
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

	perror("about to gethostbyname of localhost\n");
	
    //set our address to the telnet daemon
    struct hostent *hptr;
	if((hptr = gethostbyname("localhost")) == NULL){
		fprintf(stderr, "ERROR: Could not get address for localhost.\n");
		exit(1);
	}
	memcpy(&sout.sin_addr, hptr->h_addr, hptr->h_length);

	perror("about to bind the socket\n");
	
    //bind the server socket to the given port number
    if(bind(sock_desc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Server failed to bind port number\n");
        return 1;
    }
	
	perror("about to listen for client\n");

    if(listen(sock_desc, 1) < 0) {
        perror("Server failed listening for clients\n");
        return 1;
    }

	
	perror("about to accept connection from client\n");
    if((clientProxy = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
        perror("Server failed on accept\n");
        return 1;
    }
    
	perror("about to connect to telnet daemon\n");
	
    /* Connect to the server */
	int connectDesc = connect(telnet, (struct sockaddr*)&sout, sizeof(sout));
	if(connectDesc < 0){
		fprintf(stderr, "ERROR: could not connect to telnet daemon\n");
		exit(1);
	}

	
	perror("Connected to telnet daemon\n");
	if(telnet > clientProxy){
		largestSocket = telnet;
	}else{
		largestSocket = clientProxy;
	}
    while(1) {
        FD_ZERO(&listen1);
        FD_SET(telnet, &listen1);
        FD_SET(clientProxy, &listen1);
        timeout.tv_sec = 600;
        timeout.tv_usec = 0;
		
        nfound = select(largestSocket + 1, &listen1, (fd_set *)0, (fd_set *)0, &timeout);

        if(nfound == 0) {
            //we timed out
            //close the sockets
            close(sock_desc);
            close(clientProxy);
            close(telnet);
            exit(0);
        }
        else if(nfound < 0) {
            //something went wrong. handle the error
			perror("select returned less than 0\n");
			exit(1);
        }
        else {
            if(FD_ISSET(telnet, &listen1)) {
                //if this is set we know we must read from the telnet daemon and pass it to the clientProxy
                readString(telnet, clientProxy);
            }
            if(FD_ISSET(clientProxy, &listen1)) {
                //if this is set we must read from the clientProxy and pass it on to the telnet daemon
                readString(clientProxy, telnet);
            }
        }
    }

    return 0;

}

//a function to read in the bytes from the socket and store them into a buffer and print that string to stdout
void readString(int readSock, int writeSock) {
    //initialize the buffer
	/* char buf[500], *ptr;
    ptr = buf; */
	
	char *ptr = NULL;//##
	ptr = malloc(500);
	
    //read in the bytes
    int val = read(readSock, &ptr, 500);  
    if(val == 0){
        close(sock_desc);
        close(clientProxy);
        close(telnet);
		exit(0);
	}
	
	char *test = "testing\n";
	printf("%s", test);
	printf("Server: %s\n", ptr);
    //write to the destination socket
    int result = write(writeSock, ptr, val);
	if(result < 0){
		fprintf(stderr, "ERROR: Couldn't write size to server.\n");
		exit(1);
	}
}