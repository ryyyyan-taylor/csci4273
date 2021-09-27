//   ____ _ _            _
//  / ___| (_) ___ _ __ | |_
// | |   | | |/ _ \ '_ \| __|
// | |___| | |  __/ | | | |_
//  \____|_|_|\___|_| |_|\__|
//
// built off of provided udp example files

// allowed headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char* msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char** argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent* server;
    char* hostname;
    char buf[BUFSIZE];

    // new stuff
    char cmd[BUFSIZE];
    char fname[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    if (portno < 5001 || portno > 65534)
        error("Port must be in range 5001-65534");

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (!server) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char*) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char*)server->h_addr,
        (char*)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while (true) {
        /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("\nAvailable commands:\nget [filename]\nput [filename]\ndelete [filename]\nls\nexit\n> ");
        fgets(buf, BUFSIZE, stdin);
        sscanf(buf, "%s %s", cmd, fname);


        // GET
        if (!strncmp(cmd, "get", 3)) {
            serverlen = sizeof(serveraddr);
            
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
            if (n < 0) error("ERROR in recvfrom");

            // Receive dne from server
            if (!strncmp(buf, "dne", 3)) {
                printf("The requested file %s does not exist on the server.\n", fname);
                continue;
            }

            // send filesize in bytes
            int fileSize = atoi(buf);
            printf("Receiving file %s from server.\nSize: %d\n", fname, fileSize);
            FILE* f = fopen(fname, "wb");
            if (!f) error("Error opening file");

            int bytes = 0;
            while (true) {
                bzero(buf, sizeof(buf));
                int transferSize = BUFSIZE;

                // less than 1024 remaining
                if (fileSize - bytes < 1024) transferSize = fileSize - bytes;
                
                n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
                if (n < 0) error("ERROR in recvfrom");
                
                bytes += transferSize;
                
                printf("Recieved %d of %d bytes\n", bytes, fileSize);
                fwrite(buf, transferSize, 1, f);
                
                if (bytes >= fileSize) {
                    printf("File transferred successfully.\n");
                    fclose(f);
                    break;
                }
            }
        }


        // PUT
        else if (!strncmp(cmd, "put", 3)) {
            printf("filename: %s\n", fname);
            FILE* f = fopen(fname, "rb");
            if (!f) {
                printf("Error, no such file %s exists in server directory\n", fname);
                continue;
            }

            serverlen = sizeof(serveraddr);

            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            
            // get file size
            fseek(f, 0L, SEEK_END);
            int fileSize = ftell(f);
            rewind(f);

            char file_buf[BUFSIZE];
            sprintf(file_buf, "%d", fileSize);
            printf("Size of file: %s Bytes - sending size to server\n", file_buf);

            n = sendto(sockfd, file_buf, BUFSIZE, 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            
            bzero(file_buf, sizeof(file_buf));

            // sending file to server
            while (fread(file_buf, 1, sizeof(file_buf), f) != 0) {
                n = sendto(sockfd, file_buf, sizeof(file_buf), 0, (struct sockaddr*)&serveraddr, serverlen);
                if (n < 0) error("ERROR in sendto");
                
                bzero(file_buf, sizeof(file_buf));
            }

            fclose(f);
            printf("File %s transferred\n", fname);
        }


        // DELETE
        else if (!strncmp(cmd, "delete", 6)) {
            serverlen = sizeof(serveraddr);

            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
            if (n < 0) error("ERROR in recvfrom");
            
            // file not found
            if (!strncmp(buf, "dne", 3)) printf("Error: file %s not found on server.\n", fname);
            // else
            else printf("%s %s\n", buf, fname);
        }


        // LS
        else if (!strncmp(cmd, "ls", 2)) {
            serverlen = sizeof(serveraddr);

            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            /* print the server's reply */
            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
            if (n < 0) error("ERROR in recvfrom");

            printf("%s", buf);
        }


        // LS
        else if (!strncmp(cmd, "exit", 4)) {
            serverlen = sizeof(serveraddr);
            
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            /* print the server's reply */
            close(sockfd);
            printf("Server closed, exiting.\n");
            break;
        }


        // DEFAULT
        else {
            serverlen = sizeof(serveraddr);

            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
            if (n < 0) error("ERROR in recvfrom");
            
            printf("%s", buf);
        }
    }
    return 0;
}