/*
 * File: client.c
 * Author: Marcos Samayoa and Philip Harlow
 * Purpose: This program connects to a server, reads input from stdin,
 *  		and sends the input to the server.
 * 
 */
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <sys/socket.h>
 #include <arpa/inet.h>
 #include <string.h>
 #include <unistd.h>
 
 int main(int argc, char **argv){
	 char* serverIP;
	 int serverPort;
	 char *input = NULL;
	 size_t maxLength = 1026;
	 struct sockaddr_in sin;
	 int sockDesc;
	 
	 /* Check for the correct number of arguments before continuing*/
	 if(argc < 3){
		 fprintf(stderr, "USAGE: client <serverIp> <serverPort>\n");
		 exit(1);
	 }
	 
	 /* Get the server IP address and Port */
	 serverIP   = argv[1];
	 serverPort = atoi(argv[2]);
	 
	 sockDesc = socket(PF_INET, SOCK_STREAM, 0);
	 
	 /* Set up the struct sockaddr_in */
	 sin.sin_family = PF_INET;
	 sin.sin_port = htons(serverPort);
	 
	 in_addr_t address;
	 address = inet_addr(serverIP);
	 if(address == -1){
		 fprintf(stderr, "ERROR: Could not get address from IP.\n");
		 exit(1);
	 }
	 memcpy(&sin.sin_addr, &address, sizeof(address)); 
	 
	 /* Connect to the server */
	 int connectDesc = connect(sockDesc, (struct sockaddr*)&sin, sizeof(sin));
	 if(connectDesc < 0){
		fprintf(stderr, "ERROR: Couldn't connect to host at IP: %s  Port: %d\n", serverIP, serverPort);
		exit(1);
	 }
	 
	 /* Begin reading input from stdin and send the input to server */
	 while(getline(&input, &maxLength, stdin) != EOF){
		 strtok(input, "\n"); // Remove the Newline from input
		 
		 int sizeofPayload = strlen(input)*sizeof(char);
		 sizeofPayload = htonl(sizeofPayload);
		 
		 /* Write the size of the payload to the server */
		 int result = write(sockDesc, &sizeofPayload, sizeof(sizeofPayload));
		 if(result < 0){
			 fprintf(stderr, "ERROR: Couldn't write size to server.\n");
			 exit(1);
		 }
		 
		 /* Write the payload to the server */
		 result = write(sockDesc, input, strlen(input));
		 if(result < 0){
			 fprintf(stderr, "ERROR: Couldn't write payload to server.\n");
			 exit(1);
		 } 
		 
	 } // End while(getline())
	 
	 return 0;
 }