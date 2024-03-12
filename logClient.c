#include <stdio.h>
#include <stdlib.h> 	/* for atoi() and exit() */
#include <string.h>
	
#include <sys/socket.h> /* for socket(), bind(), connect() */
#include <sys/types.h>
#include <unistd.h> 	/* for close() */

#include <netinet/in.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */

#define BUFFSIZE 100

int main(int argc, char *argv[])
{

	/* Variables declarations */
	int sock_fd;			// socket file descriptor for the client
	
	struct sockaddr_in target_addr;	// address of the server we are going to connect to
	
	struct in_addr inp;		// structure used to store the IPv4 address in binary form
	char *serverIP;			// IP address of the server
	unsigned short serverPort;	// port number on which the server is listening
	
	char msgToSend[BUFFSIZE];	// array to store the message to send to the server
	
	/* Check correct number of arguments */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <IP_address> <listening_port>\n", argv[0]);
		exit(1);
	}
	
	serverIP = argv[1];		// first argument
	serverPort = atoi(argv[2]);	// second argument
	
	/* Check if the IPv4 address (dotted-notation) given by the user is valid */
	if (inet_aton(serverIP, &inp) == 0) {
		perror("IPv4 address not valid");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (1) Socket creation */
	
	// Creates a socket specifying domain, type and protocol. Returns a descriptor.
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket() failed");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (2) Connection to the server socket */
	
	// Construct the server address structure
	target_addr.sin_family = AF_INET;			// Internet address familiy
	target_addr.sin_addr.s_addr = inet_addr(serverIP);	// Server IP address: from numbers-and-dots notation to binary data
	target_addr.sin_port = htons(serverPort);		// Server port number
	memset(&(target_addr.sin_zero), '\0', 8);		// Zero the rest of the struct
	
	// Attempts to connect sock_fd to another socket specified by target_addr
	if (connect(sock_fd, (struct sockaddr *) &target_addr, sizeof(target_addr)) < 0) {
		perror("connect() failed");
		exit(1);
	}
	printf("Connected to the server.\n");
	
	/**********************************************************************************/
	/* (3) Send/receive data */
	
	
	/* Ask the user to insert the message to send to the log server */
	
	while (1) {
	
		printf("Enter a message to send to the log server (type 'exit' to quit): ");
		
		// fgets() reads also the newline character (\n) at the end when we press enter
		if (fgets(msgToSend, BUFFSIZE, stdin) == NULL) {
			perror("error with fgets()");
			exit(1);
		}
		
		// Remove the newline character ('\n') at the end of the string and replace it with the null terminator ('\0')
		if (msgToSend[strlen(msgToSend) - 1] == '\n') msgToSend[strlen(msgToSend) - 1] = '\0';
		
		// if the user types 'exit' we exit from the loop and send a request to the server to close the connection
		if (strcmp(msgToSend, "exit") == 0) {
			printf("Closing the connection...\n");
			send(sock_fd, "CLOSE_CONNECTION", 16, 0);
			break; // exit from while loop
		}
		
		printf("The entered message is: %s\n\n", msgToSend);
		
		// Send the message to the server
		if (send(sock_fd, msgToSend, strlen(msgToSend), 0) != strlen(msgToSend)) {
			perror("A different number of bytes was sent by send()!");
			exit(1);
		}
		
	}
	
	/**********************************************************************************/
	/* (4) Terminate the connection */
	
	close(sock_fd);
	printf("Disconnected from the server.\n");
	
	return 0;
}
