/*
Author: Philip Harlow & Marcos Samayoa
Class: cs425
Teacher: Homer
Program:  This is a server that connects to two different sockets and relays information from one socket to the other.
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
int port_number, readVal, heartbeatCount, set;
void * id;
void * save;
void readString(int readSock, int writeSock, int isHeader);
void sendHeartbeat(int clientProxy);

struct Packet {
    int type;
    int bufferLen;
    void * buffer;
};

struct Packet *header;

int main(int argc, char * argv[]) {
    fd_set listen1;
    heartbeatCount = 0;
    set = 0;
    id = malloc(sizeof(int));
    struct timeval timeout;
    int nfound;
    struct sockaddr_in sin, sout;
    int addrlen = sizeof(sin);
	int largestSocket;

    header = malloc(sizeof(struct Packet));
    save = header;

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
        perror("Invalid parameters, to run program please input as serverProxy [serverPort]\n");
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
    struct hostent *hptr;
	if((hptr = gethostbyname("localhost")) == NULL){
		fprintf(stderr, "ERROR: Could not get address for localhost.\n");
		exit(1);
	}
	memcpy(&sout.sin_addr, hptr->h_addr, hptr->h_length);
	
    //bind the server socket to the given port number
    if(bind(sock_desc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Server failed to bind port number\n");
        return 1;
    }

    if(listen(sock_desc, 5) < 0) {
        perror("Server failed listening for clients\n");
        return 1;
    }

    if((clientProxy = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
        perror("Server failed on accept\n");
        return 1;
    }
	
    /* Connect to the server */
	int connectDesc = connect(telnet, (struct sockaddr*)&sout, sizeof(sout));
	if(connectDesc < 0){
		fprintf(stderr, "ERROR: could not connect to telnet daemon\n");
		exit(1);
	}

	
    while(1) {
        if(set) {
            FD_ZERO(&listen1);
            FD_SET(telnet, &listen1);
            FD_SET(sock_desc, &listen1);
        }
        else {
            FD_ZERO(&listen1);
            FD_SET(telnet, &listen1);
            FD_SET(clientProxy, &listen1);
            FD_SET(sock_desc, &listen1);
        }
        //find which of our 2 connections is largest
        if(set) {
            if(telnet > sock_desc) {
                largestSocket = telnet;
            }
            else {
                largestSocket = sock_desc;
            }
        }
        else {
            if(telnet > clientProxy){
                largestSocket = telnet;
                if(sock_desc > largestSocket) {
                    largestSocket = sock_desc;
                }
            }
            else {
                largestSocket = clientProxy;
                if(sock_desc > largestSocket) {
                    largestSocket = sock_desc;
                }
            }
        }
        
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
		
        nfound = select(largestSocket + 1, &listen1, (fd_set *)0, (fd_set *)0, &timeout);

        if(nfound == 0) {
            //we timed out
            //minus 1 count to our heartbeat counter
            heartbeatCount--;
            set = 0;
            //if we count 3 missed heartbeat signals we want to close the connection with the client
            if(heartbeatCount <= -3 || heartbeatCount >= 3) {
                close(clientProxy);
                id = NULL;
                set = 1;
            }
            //close the sockets
            //close(sock_desc);
            //close(telnet);
            //exit(0);
        }
        else if(nfound < 0) {
            //something went wrong. handle the error
			perror("select returned less than 0\n");
			exit(1);
        }
        else {
            if(FD_ISSET(telnet, &listen1)) {
                //if this is set we know we must read from the telnet daemon and pass it to the clientProxy
                readString(telnet, clientProxy, 0); 
            }
            if(!set && FD_ISSET(clientProxy, &listen1)) {
                //if this is set we must read from the clientProxy and pass it on to the telnet daemon
                readString(clientProxy, telnet, 1);
            }
            if(FD_ISSET(sock_desc, &listen1)) {
                //this will be if we have a new connection
                if((clientProxy = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
                    perror("Server failed on accept\n");
                    return 1;
                }
                heartbeatCount = 0;
            }
        }
        sleep(1);
        sendHeartbeat(clientProxy);
    }

    return 0;

}

//a function to read in the bytes from the socket and store them into a buffer and print that string to stdout
void readString(int readSock, int writeSock, int isHeader) {
    //initialize the buffer
	/* char buf[500], *ptr;
    ptr = buf; */

    int val, result, n, len;
	char *ptr = NULL;//##
	

    if(isHeader) {
        n = sizeof(int) * 2;
        while(n > 0) {
            val = read(readSock, header, n);
            header += val;
            n = n - val;
            if(val == 0) {
                close(sock_desc);
                close(clientProxy);
                close(telnet);
                exit(0);
            }
        }
        header = save;
        header->type = ntohl(header->type);
        //header->type = header->type;
        header->bufferLen = ntohl(header->bufferLen);
        //header->bufferLen = header->bufferLen;

        len = header->bufferLen;
        if(header->type == 1) {
            heartbeatCount++;
            if(id == NULL) {
                n = len;
                void * start;
                while(n > 0) {
                    val = read(readSock, id, n);
                    id += val;
                    n = n - val;
                    if(val == 0) {
                        close(sock_desc);
                        close(clientProxy);
                        close(telnet);
                        exit(0);
                    }
                }
                id = start;
            }
        }
        else {
            ptr = malloc(len);
            n = len;
            while(n > 0) {
                val = read(readSock, ptr, n);
                n = n - val;
                if(val == 0) {
                    close(sock_desc);
                    close(clientProxy);
                    close(telnet);
                    exit(0);
                }
            }
            result = write(writeSock, ptr, len);
            if(result < 0){
                fprintf(stderr, "ERROR: Couldn't write 'message' to telnetD.\n");
                exit(1);
            }
        }
    }
    else {
        //read in the bytes
        ptr = malloc(500);
        val = read(readSock, ptr, 500);  
        if(val == 0){
            close(sock_desc);
            close(clientProxy);
            close(telnet);
            exit(0);
        }
        header->type = htonl(0);
        //header->type = 0;
        printf("%d\n", header->type);
        len = val;
        header->bufferLen = htonl(val * sizeof(char));
        //header->bufferLen = val * sizeof(char);
        header->buffer = ptr;
        //write to the destination socket
        result = write(writeSock, header, sizeof(struct Packet) - sizeof(void *));
        if(result < 0){
            fprintf(stderr, "telnet ERROR: Couldn't write 'header' to client.\n");
            exit(1);
        }
        result = write(writeSock, header->buffer, len * sizeof(char));
        if(result < 0){
            fprintf(stderr, "ERROR: Couldn't write 'buffer' to client.\n");
            exit(1);
        }
    }
}

void sendHeartbeat(int clientProxy) {
    heartbeatCount--;
    header->type = htonl(1);
    //header->type = 1;
    header->bufferLen = htonl(sizeof(int));
    //header->bufferLen = sizeof(int);
    printf("%zu\n", sizeof(int));
    header->buffer = id;
    int result = write(clientProxy, header, sizeof(struct Packet) - sizeof(void *));
    if(result < 0){
        fprintf(stderr, "telnet ERROR: Couldn't write 'header' to client.\n");
        exit(1);
    }
    result = write(clientProxy, header->buffer, sizeof(int));
    if(result < 0){
        fprintf(stderr, "ERROR: Couldn't write 'ID' to client.\n");
        exit(1);
    }
}