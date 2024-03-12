#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define THRESHOLD 20	// threshold for the size of every log file
#define MAXLOGFILE 4	// maximum number of possible log files inside the directory

int sizeExceedThreshold(char *f_name, int n_bytes, unsigned int th);
char * findMostRecentFile(char *directory);
int rotateLogs(char * dir, int maxLogs);

int main(int argc, char *argv[]) {

	char *mostRecentFile;
	char *fullpath = malloc(50);
	int ret;
	int j;
	int fd, new_fd;
	char *directory;
	
	/* Check correct number of arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		exit(1);
	}
	
	directory = argv[1];
	
	// When the server starts it has to write on the most recent file
	mostRecentFile = findMostRecentFile(directory);
	
	printf("[DEBUG] most recent file is: %s\n", mostRecentFile);
	
	/* Now, every time we want to write on the file we have to check first that the file will not exceed the threshold */
	
	ret = sizeExceedThreshold(mostRecentFile, 5, THRESHOLD);
	
	printf("[DEBUG] Its size exceed threshold: %d\n", ret);
	
	// If sizeExceedThreshold() returns 1 it means the size of the log file would exceed the threshold
	
	if (ret == 1) {
	
		// We need to create a new log file but if the directory is full (MAXLOGFILE) we have to rotate
		
		for (j = 1; j <= MAXLOGFILE; j++) {
	
		
			/*
			* Craft the name of the log file to create.
			* Concatenate the given directory path with the name of the log file.
			* The function sprintf(), instead of printing on stdout, stores the string into the specified buffer.
			*/
			sprintf(fullpath, "%s/server_%d.log", directory, j);
			
			// Test for the file existence (F_OK): if it does not exist then we found a name for a new log file
			if ((access(fullpath, F_OK)) == -1) {
				// exit from the file
				break;
			}
		}
		
		// At this point 'fullpath' will contain the name of the new log file to be created
		
		// The directory contains the maximum possible number of log files then we rotate
		if (j > MAXLOGFILE) {
		
			// Remove the oldest file and returns the descriptor of the new created one
			new_fd = rotateLogs(directory, MAXLOGFILE);
			write(new_fd, "test\n", 5);
			close(new_fd);
		}
		else {
			// We can create a new log file (there is space) and write into it
			new_fd = open(fullpath, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
			write(new_fd, "test\n", 5);
			close(new_fd);
		}
		
				
	}
	else if (ret == 0) {
	
		// Don't exceed the threshold, so we can write on the file
		fd = open(mostRecentFile, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		write(fd, "test\n", 5);
		close(fd);
	
	}
	else {
		perror("Error with sizeExceedThreshold()");
		exit(1);
	}
	
	
	free(fullpath);
	
return 0;
}



/*
* This is a function that takes 2 arguments: the number of bytes to write and the name of the file to write on.
* If the sum between the number of bytes and the size of the file is greater than a certain threshold then it returns 1.
* To read the attributes of a file we will use the system call fstat().
*/
int sizeExceedThreshold(char *f_name, int n_bytes, unsigned int th) {

	int fd;
	struct stat file_info;	// structure that contains info about the file
	
	fd = open(f_name, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open() failed");
		return -1;
	}
	
	// fstat() returns info about the file, in the struct file_info
	if ((fstat(fd, &file_info)) == -1) {
		perror("fstat() failed");
		return -1;
	}
	
	//printf("Size in bytes: %ld\n", file_info.st_size);
	// The field st_size contains the total size, in bytes (see 'man fstat' for all the fields)
	if ((file_info.st_size + n_bytes) > th)
		// Return 1 if exceed the given threshold
		return 1;
	
	if ((close(fd)) == -1) {
		perror("close failed");
		return -1;
	}
	
	// Returns 0 if don't exceed the threshold
	return 0;
}



/* This function takes a directory as argument and returns the name of the most recent file inside the directory */
char * findMostRecentFile(char *directory) {

	DIR *d;
	char *mostRecentFile = malloc(50);
	int fd;

	d = opendir(directory);
	
	// If the directory does not exist, we create it setting the permissions for the user (owner)
	if (d == NULL) {
		// Create the directory
		if (mkdir(directory, S_IRUSR|S_IWUSR|S_IXUSR) == -1) {
			perror("mkdir() failed");
			exit(1);
		}
		
		printf("[+] Directory \'%s\' successfully created.\n", directory);
		
		// The directory is empty, so we create a log file
		sprintf(mostRecentFile, "%s/%s", directory, "server_1.log");
		return mostRecentFile;
	}
	else {
		/* 
		* The directory exists, we need to search for the most recent file.
		* The most recent file will always be the last one (the one with the highest number), 
		* so we loop starting from the last one.
		*/
		for (int i=MAXLOGFILE; i>0; i--) {
		
			sprintf(mostRecentFile, "%s/server_%d.log", directory, i);
			
			// Check the existence of the file: if it exists then we found the most recent file
			if ((access(mostRecentFile, F_OK)) == 0) 
				break; // exit from the loop
			
		}
		
		/* 
		* If we exit from the for loop because of the break, then 'mostRecentFile' contains the name of the most recent
		* file inside the directory. Otherwise, if we executed all the iterations, it means the directory exists but 
		* it is empty, then at the end of the for loop 'mostRecentFile' contains 'server_1.log' and we create it.
		*/
		fd = open(mostRecentFile, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		return mostRecentFile;
	}

}

/*
When the server is started it will write to the most recent log file inside the given directory.
If the given directory is empty, the first log file 'server_1.log' is created.
When server_1.log exceeds a given threshold, the file server_2.log is created.
When server_2.log exceeds the threshold, the file server_3.log is created.
When server_3.log exceeds the threshold, we reached the maximum number of possible log files (3 in this example)
so we remove the oldest log file (server_1.log), we rename the files (2 becomes 1, 3 becomes 2) and
finally we create a new log file (server_3.log).
When the server is started again, it will write to the most recent file (which will always be the last one).
*/


/* 
* Rotate logs 
* This function takes 2 inputs: a directory and the maximum number of log files that the directory can contain.
* This function is executed whene we reached the maximum number of possible log files inside the directory.
* It deletes the oldest log file (which is always server_1.log), renames the files, and creates a new log file.
* Returns the file descriptor of the new log file.
*/
int rotateLogs(char * dir, int maxLogs) {

	int fd;
	char newLogFile[50];
	char fileToRemove[50];

	
	// 1) Delete oldest log file (it is always server_1.log)
	sprintf(fileToRemove, "%s/server_1.log", dir);
	remove(fileToRemove);
	
	// 2) Rename log files (E.g., 2,3 becomes 1,2)
	for (int i = 0; i < maxLogs-1; i++) {
	
		char currentLogFile[50];
		char newFileName[50];
		sprintf(currentLogFile, "%s/server_%d.log", dir, (2+i));
		sprintf(newFileName, "%s/server_%d.log", dir, (2+i)-1);
		
		rename(currentLogFile, newFileName);
	}
	
	// 3) Create new log file
	sprintf(newLogFile, "%s/server_%d.log", dir, maxLogs);
	fd = open(newLogFile, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open() failed");
		exit(1);
	}
	
	// Return the file descriptor of the newly created log file
	return fd;
}
