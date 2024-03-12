# logServer
This repository contains the materials I submitted for the homework project of the "Computer Systems & Programming" university course held during the academic year 2023/24.
The objective of the project was to implement a "log server" in C,  i.e. a server that writes to a log file the inputs it receives from the clients.

# Explanation of the files contained in the repository
- **logClient.c**<br>
It is the source code for a client that contacts the server. In particular, the server is able to manage an unlimited number of clients. It means that it is a “concurrent server” (a server that is able to handle multiple clients at the same time), so we can run multiple instances of clients and test that the server works properly.
- **logServer.c**<br>
It is the source code for the server.
- **logsRotation.c**<br>
This file contains my implementation of the logs rotation mechanism. Here is the specification to implement: "When the log file size exceed a given threshold, the server should cancel the oldest log file in the log directory and create a new log file. In this case, the server should not create a new log file at start-up, but rather append to the most recent log file in the directory."
- **projectReport.pdf**<br>
This report explains how I solved the main problems encountered in the implementation of such server and why I made certain choices. The main problems were: file locking, signal handling and logs rotation.
- **softwareArchitecture.pdf**<br>
This pdf explains pictorially the main idea behind the software architecture: to implement such server I used the fork() system call to create a dedicated child process to handle each client. This mechanism is the same one used by inetd (internet service daemon).
- **userManual.pdf**<br>
This pdf contains the commands to properly run the server and the clients.
