/*
Author: Philip Harlow & Marcos Samayoa
Class: cs425
Teacher: Homer
Program:  This is a client that connects to two different sockets and relays information from one to the other.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>


int sock_desc, serverProxy, telnetDaemon;  //telnetDaemon will be the socket listeing to the localhost telnet daemon, serverProxy will be listing to the clientProxy
int port_number_clientProxy, port_number_serverProxy, readVal;
int hbDiff = 0; // The difference between heartbeats sent and recieved.
void readString(int readSock, int writeSock, int isHeader);
void sendHeartbeat(int writeSock, int SID);
void delay(int timeToDelay);


struct Packet{
	int type;      // 0 or 1, 0 = data. 1 = heartbeat
	int length;    // length of the payload
	void *payload; 
};

struct Packet *ptr;

int main(int argc, char * argv[]) {
    fd_set listen1;
    struct timeval timeout;
    ptr = malloc(sizeof(struct Packet));
    int nfound;
    struct sockaddr_in sin, sout;
    int addrlen = sizeof(sin);
	int largestSocket;
	time_t t; 
	srand ((unsigned) time(&t));
	int SID = rand() % 10000;  // SID is between 0 and 10000
	

    //create socket file descriptor
    if((sock_desc = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }
    if((telnetDaemon = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server failed to create socket\n");
        return 1;
    }

    //Check to see if the user inputed all the required arguments, if not terminate the program
    if(argc < 4) {
        perror("Invalid parameters, to run program please input as:\n");
        perror("\tclientProxy 5200 [serverIP] 6200\n");
        perror("\t[serverIP] is the ipAddress of the machine your trying to connect\n");
        return 1;
    }

    //convert command line argument to int
    port_number_clientProxy = atoi(argv[1]);
    //convert command line argument to int
    port_number_serverProxy = atoi(argv[3]);


    //set the protocol to PF_INET
    sin.sin_family = PF_INET;
    sout.sin_family = PF_INET;

    //set the port number to the specified port from command line
    sin.sin_port = htons(port_number_clientProxy);
    sout.sin_port = htons(port_number_serverProxy);

    //set the socket to accept connections from any ip
    sin.sin_addr.s_addr = INADDR_ANY;

    //set the address of the serverProxy to connect to
    in_addr_t address;
	address = inet_addr(argv[2]);
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

	/* Connect to the serverProxy */
	int connectDesc = connect(telnetDaemon, (struct sockaddr*)&sout, sizeof(sout));
	if(connectDesc < 0){
		fprintf(stderr, "ERROR: could not connect to telnet daemon\n");
		exit(1);
	}
	
    if(listen(sock_desc, 1) < 0) {
        perror("Server failed listening for clients\n");
        return 1;
    }

    if((serverProxy = accept(sock_desc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
        perror("Server failed on accept\n");
        return 1;
    }
    
	if(telnetDaemon > serverProxy){
		largestSocket = telnetDaemon;
	}else{
		largestSocket = serverProxy;
	}
    while(1) {
		/* Send a heartbeat message */
		sendHeartbeat(serverProxy, SID);
		hbDiff++;
		
		/* If we reach a heartbeat difference of 3, the server has stopped
		   responding. Close the socket for the server. Try to reconnect to the server 
		   using a new socket. */
		if(hbDiff >= 3){
			fprintf(stderr, "Connection with server lost.\n");
			close(serverProxy);  // Close the old connection
			
			/* Attempt a new connection to the server. */
			int newSockDesc = socket(PF_INET, SOCK_STREAM, 0);
			
			//bind the server socket to the given port number
			if(bind(newSockDesc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
				perror("Server failed to bind port number\n");
				//return 1;
			}
			
			if(listen(newSockDesc, 1) < 0) {
				perror("Server failed listening for clients\n");
				//return 1;
			}
			if((serverProxy = accept(newSockDesc, (struct sockaddr *)&sin,(socklen_t*)&addrlen)) < 0) {
				perror("Server failed on accept\n");
				//return 1;
			}
			else{
				printf("Connected to Server.\n");
				hbDiff = 0;
			}
			
		}
		
		
        FD_ZERO(&listen1);
        FD_SET(telnetDaemon, &listen1);
        FD_SET(serverProxy, &listen1);
        timeout.tv_sec = 600;
        timeout.tv_usec = 0;

        nfound = select(largestSocket + 1, &listen1, (fd_set *)0, (fd_set *)0, &timeout);

        if(nfound == 0) {
            //we timed out
            //close the sockets
            close(sock_desc);
            close(serverProxy);
            close(telnetDaemon);
            exit(0);
        }
        else if(nfound < 0) {
            //something went wrong. handle the error
			perror("select returned less than 0\n");
			exit(1);
        }
        else {
            if(FD_ISSET(telnetDaemon, &listen1)) {
                //if this is set we know we must read from the telnet daemon and pass it to the serverProxy
                //we pass an additional variable that represents if the stream to read from has a header on it
                readString(telnetDaemon, serverProxy, 0);
            }
            if(FD_ISSET(serverProxy, &listen1)) {
                //if this is set we must read from the serverProxy and pass it on to the telnet daemon
                //we pass an additional variable that represents if the stream to read from has a header on it
                readString(serverProxy, telnetDaemon, 1);
            }
        }
		
		delay(1); // Wait 1 second before sending a heartbeat and checking select
    }
    return 0;

}

//a function to read in the bytes from the socket and store them into a buffer and print that string to stdout
void readString(int readSock, int writeSock, int isHeader) {
    //initialize the buffer
    char * str = NULL;
    int val, result, n;

	
	// Possibly need to malloc ptr.payload = malloc(500);
	// 		remember Packet.payload is set to payload[500];
	
    //read in the bytes
    if(isHeader) {
        n = sizeof(int) * 2;
        while(n > 0) {
            val = read(readSock, &ptr, n);
            n = n - val;
            if(val == 0) {
                close(sock_desc);
                close(serverProxy);
                close(telnetDaemon);
                exit(0);
            }
        }
        if(ptr->type == 1) {
            hbDiff--;
        }
        else {
            str = malloc(ptr->length);
            n = ptr->length;
            while(n > 0) {
                val = read(readSock, str, n);
                n = n - val;
                if(val == 0) {
                    close(sock_desc);
                    close(serverProxy);
                    close(telnetDaemon);
                    exit(0);
                }
            }
            result = write(writeSock, ptr, ptr->length);
            if(result < 0){
                fprintf(stderr, "ERROR: Couldn't write 'message' to telnetD.\n");
                exit(1);
            }
        }
    }
    else {
        //read in the bytes
        str = malloc(500);
        val = read(readSock, str, 500);  
        if(val == 0){
            close(sock_desc);
            close(serverProxy);
            close(telnetDaemon);
            exit(0);
        }
        ptr->type = 0;
        ptr->length = val * sizeof(char);
        ptr->payload = str;
        //write to the destination socket
        result = write(writeSock, &ptr, sizeof(int) * 2);
        if(result < 0){
            fprintf(stderr, "ERROR: Couldn't write 'header' to client.\n");
            exit(1);
        }
        result = write(writeSock, ptr->payload, ptr->length);
        if(result < 0){
            fprintf(stderr, "ERROR: Couldn't write 'buffer' to client.\n");
            exit(1);
        }
    }
    
}


/* sendHeartbeat(): This function sends a heartbeat message to the
 *					serverProxy. 
 *
 *		writeSock: The socket to write to.
 * 			  SID: The session ID to write in the payload of the message.
 */
void sendHeartbeat(int writeSock, int SID){
	ptr->type = 1;   // type = 1 means this is a heartbeat message
    
	ptr->payload = &SID;
	//printf("Payload: %s\n", p.payload);
	ptr->length = sizeof(ptr->payload);
	
	write(writeSock, &ptr, sizeof(ptr) - sizeof(void *));
	write(writeSock, ptr->payload, ptr->length);
	return;
}

/* delay(): This function delays the program from continuing
 * 			until a certain amount of time has passed.
 *
 *		timeToDelay: this is the amount of time (in Seconds) to wait until continuing
 *
 */
void delay(int timeToDelay){
	time_t startingTime = time(NULL);
	
	while(1){
		time_t currTime = time(NULL);
		if(startingTime+timeToDelay >= currTime){
			return;
		}
	}
}