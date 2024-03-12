#include <stdio.h>
#include <stdlib.h> 	/* for atoi() and exit() */
#include <string.h>	/* for memset() */
#include <signal.h>
#include <time.h>

#include <sys/socket.h> /* for socket(), bind(), connect() */
#include <sys/types.h>
#include <sys/wait.h>	/* for waitpid() */
#include <unistd.h> 	/* for close() */

#include <netinet/in.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */

#include <dirent.h>
#include <fcntl.h>	/* for the flags to set the access mode  */
#include <sys/stat.h>	/* for the flags to define the file permissions */

#define MAXQUEUE 3
#define MAXLOGFILE 5		// maximum number of log files in the given directory

// Global variables
char *directory;		// directory to store the log file (global to be accessible by sign. handler)
char fullpath[50];		// full path to the log file (global to be accessible by sign. handler)
int is_main_process = 1;	// to distinguish between parent and child

// Helper function to compute the current time to put in the log file
char * get_timestamp(char *t, time_t mt);

// Function that logs the received message inside the log file, implementing the advisory locking mechanism
int logReceivedMessage(char *pathToFile, char *message, char *time, char *addr, int pn);

// Function similar to the one above, used to log a general message in the log file
int logMessage(char *pathToFile, char *message, char *time);

// Signal handler for the SIGINT signal (Ctrl+C)
void shutdown_handler(int signum);

int main(int argc, char *argv[])
{

	/* Variables declarations */
	unsigned int i;				// counter to give the name to the log file			
	
	int serverSocket;			// socket descriptor for the server
	int newSocket;				// socket descriptor for the client
	
	struct sockaddr_in server_address;	// local address of the server
	struct sockaddr_in client_address;	// address of the client
	
	unsigned short serverPort;		// port on which the server will listen
	
	DIR *d;
	
	unsigned int clientAddrLength;		// length of client_address structure
	
	pid_t processID;			// Process ID returned by fork()
	
	char buffer[1024];			// buffer where to write the data read by recv()
	int recv_length;			// number of bytes written in the buffer by recv()
	
	time_t mytime;				// to get the timestamp for the log file
	char *t;

	/* Check correct number of arguments */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <listening_port> <directory>\n", argv[0]);
		exit(1);
	}
	
	serverPort = atoi(argv[1]);		// first argument
	directory = argv[2];			// second argument
	
	/**********************************************************************************/
	
	/* Check if the given directory exists */
	
	d = opendir(directory);
	// If it does not exist, we create it setting the permissions for the user (owner)
	if (d == NULL) {
		if (mkdir(directory, S_IRUSR|S_IWUSR|S_IXUSR) == -1) {
			perror("mkdir() failed");
			exit(1);
		}
		printf("[+] Directory \'%s\' successfully created.\n", directory);
	}
	
	/**********************************************************************************/
	
	/* When started the server should open a new log file (without removing any old file) */
	
	for (i = 0; i <= MAXLOGFILE; i++) {
	
		
		/*
		* Craft the name of the log file.
		* Concatenate the given directory path with the name of the log file.
		* The function sprintf(), instead of printing on stdout, stores the string into the specified buffer.
		*/
		sprintf(fullpath, "%s/server_%d.log", directory, i);
		
		// Test for the file existence (F_OK): if it does not exist then we found a name for a new log file
		if ((access(fullpath, F_OK)) == -1) {
			// exit from the for
			break;
		}
	}
	
	/*
	* At this point 'fullpath' will contain the name of the new log file to be created.
	* It will be created for the first time whenever the functions logReceivedMessage() or logMessage() are called.
	*/
	
	// If the directory contains the maximum number of possible log files then we terminate.
	// 'i' will be equal to MAXLOGFILE only if we did not exit from the for loop through the break.
	if (i == MAXLOGFILE) {
		perror("Max number of log files reached!");
		exit(1);
	}
	
	/**********************************************************************************/
	
	// Register the signal handler for SIGINT signal
	if (signal(SIGINT, shutdown_handler) == SIG_ERR) {
		perror("signal() failed");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (1) Create a socket for incoming connections */
	
	// Creates a socket specifying the domain (Internet), the type (Stream) and protocol (0 for TCP). Returns a descriptor.
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket() failed");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (2) Bind the socket to an IP address and to the port number specified in the command line */
	
	// Construct local address structure
	server_address.sin_family = AF_INET;		// Internet address familiy
	server_address.sin_addr.s_addr = INADDR_ANY;	// Any address of the machine
	server_address.sin_port = htons(serverPort);
	memset(&(server_address.sin_zero), '\0', 8); 	// Zero the rest of the struct
	
	// Bind to the local address
	if (bind(serverSocket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		perror("bind() failed");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (3) Listen for incoming connections from clients */
	
	// specify willingness to accept incoming connections and a queue limit for pending connections
	if (listen(serverSocket, MAXQUEUE)) {
		perror("listen() failed");
		exit(1);
	}
	
	/**********************************************************************************/
	/* (4) Accept a connection / Wait for a client to connect */
	
	for (;;) { // Accept loop
	
		// size of client_address structure
		clientAddrLength = sizeof(client_address);
		
		/*
		* Returns a new socket file descriptor for the accepted connection. 
		* The connecting client's address information are written into the structure client_address.
		*/
		if ((newSocket = accept(serverSocket, (struct sockaddr *) &client_address, &clientAddrLength)) < 0) {
			perror("accept() failed");
			exit(1);
		}
		
		// newSocket is now connected to a client
		printf("Server: got connection from %s port %d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
		
		t = get_timestamp(t, mytime);
		// Record the new connection on the log file
		if ((logReceivedMessage(fullpath, "NEW Connection established", t, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port))) == -1) {
					perror("Error while logging the new connection");
					exit(1);
				}
		
	/**********************************************************************************/
		
		// Fork a child process: it will handle the client requests
		
		if ((processID = fork()) < 0) {
			perror("fork() failed");
			exit(1);
		}
		else if (processID == 0) { // if this is the child process
		
			// Set to 0 (for the signal handler)
			is_main_process = 0;
			
			// Child closes parent socket
			close(serverSocket); 
			
			/**********************************************************************************/
			
			/* Child handles the client *	
			
			/* Loop that receives data from the connected client and prints it out. */
			while (1) {
			
				/*
				* The recv() function is given a pointer to a buffer and a maximum length to read from
				* the socket. The function writes the data into the buffer passed to it and returns the
				* number of bytes it actually wrote.
				*/
				if ((recv_length = recv(newSocket, buffer, 1024, 0)) < 0) {
					perror("recv() failed");
					exit(1);
				}
			
				// Check if the client requested to close the connection
				if (strcmp(buffer, "CLOSE_CONNECTION") == 0) {
					printf("Received close signal. Closing connection...\n");
					t = get_timestamp(t, mytime);
					// Log the disconnection
					if ((logReceivedMessage(fullpath, buffer, t, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port))) == -1) {
					perror("Error while logging the disconnection");
					exit(1);
				}
					
					break; // exit from while loop to disconnect
				}
				
				// Print the received number of bytes
				printf("RECV: %d bytes\n", recv_length);
				
				t = get_timestamp(t, mytime);
				
				// print time, client address and port number
				printf("%s | from %s port %d --> ", t, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
				
				/*
				* Print the received message.
				* By using %.*s, we provide the width as an argument, ensuring that only the
				* specified number of data (the length of the received data) are printed.
				* This is a safe way to handle non null-terminated string.
				*/
				printf("%.*s\n\n", (int)recv_length, buffer);
				
				// Log the message inside the log file
				if ((logReceivedMessage(fullpath, buffer, t, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port))) == -1) {
					perror("Error while logging the received message");
					exit(1);
				}
				
				/*
				* Clear the buffer array before using it to receive other data from the client.
				* By setting all the bytes to zero, we ensure that there will not be any previous
				* data left in the buffer.
				*/
				memset(buffer, 0, sizeof(buffer));
			}
			
			// child closes client socket
			close(newSocket);
			
			printf("Disconnected from %s:%d\n\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
			
			/**********************************************************************************/
			
			// child process terminates
			exit(0);
		}
		else { // This is the parent process
		
			// Parent closes client socket descriptor
			close(newSocket);
			
			/*
			* To avoid zombie processes (processes that have terminated but still have an entry in the process table)
			* and properly release the resources, the parent process should wait for its child process.
			*/
			
			// Wait for any terminated child process, without storing status info, return immediately if no child has exited (non-blocking wait)
			if ((waitpid(-1, NULL, WNOHANG)) < 0) {
				perror("waitpid() failed");
				exit(1);
			}
		
		}
		
	} // end of for loop
	
	/**********************************************************************************/
	
	// Never executed
	return 0;
}


/***********************************************************************************************************/

// Helper function to compute the current time to put in the log file
char * get_timestamp(char *t, time_t mt) {

	// get current time in seconds
	time(&mt);
	// ctime() returns a pointer to a string which ends with a new line character
	t = ctime(&mt);
	// remove the new line character at the end
	if (t[strlen(t)-1] == '\n') t[strlen(t)-1] = '\0';
	
	return t;
}



/* 
* This function appends a message to the file specified as an argument only if no other process holds a lock on the file. 
* In particular, using fcntl(), if another process holds a lock on the file, the caller waits for that process to release
* the lock, otherwise the function acquires a lock on the entire file, append a string to it, release the lock and terminates.
*/
int logReceivedMessage(char *pathToFile, char *message, char *time, char *addr, int pn) {
	
	int fd;
	struct flock lock;
	char line[250];		// a single line of the log file
	
	/*
	* Opening file
	* The first set of flags is used to set the access mode:
	* O_WRONLY --> open file for write-only access
	* O_CREAT --> create the file if it doesn't exist
	* O_APPEND --> write data at the end of the file
	* The second set of flags is used to define the file permissions of the newly created file
	* S_IRUSR --> give the file read permission for the user (owner)
	* S_IWUSR --> give the file write permission for the user (owner)
	*/
	fd = open(pathToFile, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("Error opening the log file");
		return -1;
	}
	
	// Try to acquire a lock
	lock.l_type = F_WRLCK;		// Write Lock 
	lock.l_whence = SEEK_SET;	// Start of the file (how to interpret l_start)
	lock.l_start = 0;		// Offset at start of the file
	lock.l_len = 0;			// Special value: lock all bytes
	
	/*
	* By using F_SETLKW, if another process holds the lock, 
	* the fcntl() call will block until the lock can be acquired,
	* instead of immediately returning an error.
	*/
	if (fcntl(fd, F_SETLKW, &lock) == -1) {
		perror("Error acquiring the lock");
		close(fd);
		return -1;
	}
	
	// At this point, the lock has been acquired
	
	// Craft a single line of the log by concatenating different info
	sprintf(line, "%s | from %s port %d --> %s\n", time, addr, pn, message);
	
	// Append the line to the log file
	if (write(fd, line, strlen(line)) == -1) {
		perror("write() failed");
		return -1;
	}
	
	// Release the lock
	lock.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &lock) == -1) {
		perror("Error releasing the lock");
		close(fd);
		return -1;
	}
	
	// At this point the lock has been released
	
	close(fd); // close the file
	return 0;

}


/* This function is similar to logReceivedMessage() but takes less arguments. It is used by the signal handler to log the shutdown */
int logMessage(char *pathToFile, char *message, char *time) {
	
	int fd;
	struct flock lock;
	char line[250];			// a single line of the log file
	
	// Opening file
	fd = open(pathToFile, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("Error opening the file");
		return -1;
	}
	
	// Try to acquire a lock
	lock.l_type = F_WRLCK;		// Write Lock 
	lock.l_whence = SEEK_SET;	// Start of the file (how to interpret l_start)
	lock.l_start = 0;		// Offset at start of the file
	lock.l_len = 0;			// Special value: lock all bytes
	
	if (fcntl(fd, F_SETLKW, &lock) == -1) {
		perror("Error acquiring the lock");
		close(fd);
		return -1;
	}
	
	// At this point, the lock has been acquired
	
	// Craft a single line of the log by concatenating different info
	sprintf(line, "%s | %s\n", time, message);
	
	// Append the line to the log file
	if (write(fd, line, strlen(line)) == -1) {
		perror("write() failed");
		return -1;
	}
	
	// Release the lock
	lock.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &lock) == -1) {
		perror("Error releasing the lock");
		close(fd);
		return -1;
	}
	
	// At this point the lock has been released
	
	close(fd); // close the file
	return 0;

}



/* 
* When the SIGINT signal is received this signal handler will be executed both by the main process and the child
* processes. However, only the main process will handle the shut down.
*/
void shutdown_handler(int signum) {

	if (is_main_process == 0) {
		// The child process will only terminate
		exit(0);
	}
	else {
	
		time_t mytime;
		char *t;
		
		t = get_timestamp(t, mytime);
		
		// Record the order of shutdown in the log file (fullpath is a global variable)
		if (logMessage(fullpath, "The server was shut down", t) == -1) {
			perror("Error while logging the shutdown message");
			exit(1);
		}
		
		// Use write() instead of printf() because write() is an async-signal-safe function
		write(1, "\nShutting down the server... Goodbye!\n", 38); // 1 is the file descriptor for stdout
		exit(0);
	}
}
