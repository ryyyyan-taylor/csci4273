//  ____
// / ___|  ___ _ ____   _____ _ __
// \___ \ / _ \ '__\ \ / / _ \ '__|
//  ___) |  __/ |   \ V /  __/ |
// |____/ \___|_|    \_/ \___|_|
//
// built off of provided udp example files

// allowed headers
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char* msg) {
	perror(msg);
	exit(1);
}


int main(int argc, char** argv) {

	int sockfd, portno, clientlen;
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent* hostp; /* client host info */
	
	char buf[BUFSIZE]; /* message buf */
	char cmd[BUFSIZE];
	char fname[BUFSIZE];
	char* hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */


	/* 
     * check command line arguments 
     */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);
	if (portno < 5001 || portno > 65534)
		error("Port must be in range 5001-65534");

	/* 
     * socket: create the parent socket 
     */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");


	// provided setsockopt for immediate revival
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
	           (const void*)&optval, sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char*) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/*
	 * bind: associate the parent socket with a port
	 */
	if (bind(sockfd, (struct sockaddr*) &serveraddr,
	         sizeof(serveraddr)) < 0)
		error("ERROR on binding");


	clientlen = sizeof(clientaddr);
	while (true) {

		// receive from client
		bzero(buf, BUFSIZE);
		n = recvfrom(sockfd, buf, BUFSIZE, 0,
		             (struct sockaddr*) &clientaddr, &clientlen);
		if (n < 0) error("ERROR in recvfrom");

		// authenticate who sent what
		hostp = gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr,
		                      sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if (hostp == NULL)
			error("ERROR on gethostbyaddr");
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
			error("ERROR on inet_ntoa\n");
		printf("server received datagram from %s (%s)\n",
		       hostp->h_name, hostaddrp);
		printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);

		sscanf(buf, "%s %s", cmd, fname);


		// GET
		if (!strncmp(cmd, "get", 3)) {
			bzero(buf, sizeof(buf));
			clientlen = sizeof(clientaddr);
			printf("Retrieving file %s\n", fname);
			FILE* f = fopen(fname, "rb");
			
			if (!f) {

				strncpy(buf, "dne", 3);
				printf("Error: No such file %s exists in server directory.\n", fname);
				
				n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
				
				if (n < 0) error("ERROR in sendto");
				
				continue;
			}

			// Calculate file size to tell the client how many bytes to expect
			fseek(f, 0L, SEEK_END);
			int fSize = ftell(f);
			rewind(f);
			sprintf(buf, "%d", fSize);
			printf("Size of file: %s Bytes - sending size to server\n", buf);

			n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
			if (n < 0) error("ERROR in sendto");
			bzero(buf, sizeof(buf));

			//Send file to client 1024 bytes at a time
			while (fread(buf, 1, sizeof(buf), f)) {
				n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
				if (n < 0) error("ERROR in sendto");
				bzero(buf, sizeof(buf));
			}
			fclose(f);
		}


		// PUT
		else if (!strncmp(cmd, "put", 3)) {
			printf("Receiving file %s.\n", fname);

			n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr*)&clientaddr, &clientlen);
			if (n < 0) error("ERROR in recvfrom");

			printf("Size: %s\n", buf);
			int fSize = atoi(buf);
			FILE* f = fopen(fname, "wb");
			if (f == NULL)
				error("Error opening put filename");

			int transBytes = 0;
			while (true) {
				bzero(buf, sizeof(buf));
				int transSize = BUFSIZE;
				if (fSize - transBytes < 1024) {
					//Less than 1024 bytes remaining, don't send full buffer
					transSize = fSize - transBytes;
				}
				n = recvfrom(sockfd, buf, transSize, 0, (struct sockaddr*)&clientaddr, &clientlen);
				if (n < 0) error("ERROR in recvfrom");

				transBytes += transSize;
				printf("Received %d of %d bytes\n", transBytes, fSize);
				
				fwrite(buf, transSize, 1, f);
				
				if (transBytes >= fSize) {
					printf("File %s transferred.\n", fname);
					fclose(f);
					break;
				}
			}
		}


		// DELTE
		else if (!strncmp(cmd, "delete", 6)) {
			printf("Deleting file %s\n", fname);
			bzero(buf, sizeof(buf));
			FILE* f = fopen(fname, "rb");
			
			// File not found
			if (!f) {
				strncpy(buf, "dne", 3);
				printf("Error: File %s not found.\n", fname);
				
				n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
				if (n < 0) error("ERROR in sendto");
				
				continue;
			}

			fclose(f);

			n = remove(fname);
			if (n < 0) error("Error in remove");

			printf("Deleted file %s\n\n", fname);
			strcpy(buf, "Deleted file");
			n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
			if (n < 0) error("ERROR in sendto");
		}


		// LS
		else if (!strncmp(cmd, "ls", 2)) {
			bzero(buf, sizeof(buf));
			DIR* d = opendir(".");
			while (true) {

				// find and add all files to buffer
				struct dirent* dir = readdir(d);
				if (!dir) break;

				strcat(buf, dir->d_name);
				strncat(buf, "\n", 1);
			}

			n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
			if (n < 0) error("ERROR in sendto");
		}


		// EXIT
		else if (!strncmp(cmd, "exit", 4)) {
			printf("Received exit from client\n");
			break;
		}


		// DEFAULT
		else {
			printf("Command %s not found, echoing back.\n", cmd);
			bzero(buf, strlen(buf));
			sprintf(buf, "Command %s not found\n", cmd);

			n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &clientaddr, clientlen);
			if (n < 0) error("ERROR in sendto");
		}
	}
	return 0;
}