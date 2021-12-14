//  ____  _____   ____                           
// |  _ \|  ___| / ___|  ___ _ ____   _____ _ __ 
// | | | | |_    \___ \ / _ \ '__\ \ / / _ \ '__|
// | |_| |  _|    ___) |  __/ |   \ V /  __/ |   
// |____/|_|     |____/ \___|_|    \_/ \___|_|   
//       
// Distributed file server : PA4
// Ryan Taylor

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 8192
#define BUF 256
#define LISTENQ 1024

char path[6];

int open_listenfd (int port);
void process (int connfd);
void* thread (void* vargp);
void createUserDir (char* user);
bool auth (char* msg, char* userBuf);


int main (int argc, char** argv) {
    int listenfd, * connfdp, portno, clientlen = sizeof (struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 3) {
        printf ("Correct usage: ./dfs </DFS{1-4}> <port #>\n;");
        return 1;
    }

    if (strcmp (argv[1], "/DFS1") && strcmp (argv[1], "/DFS2") && strcmp (argv[1], "/DFS3") && strcmp (argv[1], "/DFS4")) {
        printf ("Correct usage: ./dfs </DFS{1-4}> <port #>\n");
        return 1;
    }

    portno = atoi (argv[2]);
    strcpy (path, argv[1]);
    listenfd = open_listenfd (portno);

    // main loop, create threads for each connection
    while (1) {
        connfdp = malloc (sizeof (int));
        *connfdp = accept (listenfd, (struct sockaddr*) &clientaddr, &clientlen);
        pthread_create (&tid, NULL, thread, connfdp);
    }
}

void* thread (void* vargp) {
    printf ("Process %d born\n", pthread_self ());
    int connfd = *((int*) vargp);
    pthread_detach (pthread_self ());
    free (vargp);
    process (connfd);
    close (connfd);
    printf ("Process %d done\n", pthread_self ());
    return NULL;
}

void process (int connfd) {
    char user[BUF];
    while (1) {
        size_t n;
        char buf[MAXLINE];
        bzero (buf, MAXLINE);

        n = read (connfd, buf, MAXLINE);
        if (n == 0) break;
        printf ("\nReceived: %s\n", buf);

        // user authentication
        if (!strncmp (buf, "auth", 4)) {
            if (auth (buf, user)) write (connfd, "auth", 4);
            else write (connfd, "noauth", 6);
        }


        // GET
        else if (!strncmp (buf, "get", 3)) {
            struct stat s;
            int fsize;
            char* fname = strchr (buf, ' ') + 1;
            char filepath[strlen (fname) + strlen (path) + strlen (user) + 4];
            strcpy (filepath, ".");
            strcat (filepath, path);
            strcat (filepath, "/");
            strcat (filepath, user);
            strcat (filepath, "/");
            strcat (filepath, fname);

            createUserDir (user);
            printf ("Checking for file %s\n", filepath);
            if (stat (filepath, &s) == 0) {
                fsize = (int) s.st_size;
                if (fsize == 0) {
                    printf ("File not found\n");
                    write (connfd, "dne", 4);
                    continue;
                }
                printf ("File size: %d\n", fsize);

                char str[128];

                sprintf (str, "fsize:%d", fsize);
                printf ("Send filesize %s to client\n", str);
                write (connfd, str, strlen (str));
            }
            else {
                printf ("File not found\n");
                write (connfd, "dne", 4);
                continue;
            }

            char chunk[fsize];
            memset (chunk, 0, fsize);
            FILE* f = fopen (filepath, "rb");
            if (!f) {
                printf ("Error opening file\n");
            }
            fread (chunk, fsize, 1, f);
            fclose (f);

            char ready_buf[6];
            read (connfd, ready_buf, 6);
            write (connfd, chunk, fsize);
        }


        // PUT
        else if (!strncmp (buf, "put", 3)) {
            createUserDir (user);
            char* fname = strchr (buf, ' ') + 1;
            *(fname - 1) = '\0';
            char* fsize = strchr (buf, ':') + 1;
            int fileSize = atoi (fsize);
            printf ("File size: %d\n", fileSize);

            // build full path to file chunks
            char dir[strlen (path) + strlen (fname) + strlen (user) + 5];
            bzero (dir, sizeof (dir));
            strcpy (dir, ".");
            strcat (dir, path);
            strcat (dir, "/");
            strcat (dir, user);
            strcat (dir, "/");
            strcat (dir, fname);

            printf ("Final file path: %s\n", dir);
            FILE* f, * tst;
            f = fopen (dir, "ab+");
            if (!f) {
                printf ("Error opening file\n");
                continue;
            }

            char filebuf[fileSize];

            n = read (connfd, filebuf, fileSize);
            printf ("Received %d bytes:\n", (int) n);
            if (n < 0) {
                printf ("Error in put:read\n");
                break;
            }
            fwrite (filebuf, 1, n, f);
            memset (filebuf, 0, sizeof (filebuf));

            fclose (f);
        }


        // LIST
        else if (!strncmp (buf, "list", 2)) {
            // make sure user directories actually exist
            createUserDir (user);

            memset (buf, 0, sizeof (buf));
            char fpath[strlen (path) + strlen (user) + 3];
            memset (fpath, 0, sizeof (fpath));

            strcpy (fpath, ".");
            strcat (fpath, path);
            strcat (fpath, "/");
            strcat (fpath, user);

            printf ("Opening directory %s\n", fpath);
            DIR* d = opendir (fpath);
            while (true) {
                struct dirent* dir = readdir (d);
                if (dir == NULL) {
                    break;
                }
                if (strcmp (dir->d_name, ".") && strcmp (dir->d_name, "..")) { //Ignore "." and ".."
                    strcat (buf, dir->d_name);
                    strcat (buf, "\n");
                }
            }
            closedir (d);

            if (strlen (buf) == 0) write (connfd, "nf", 2);
            else write (connfd, buf, strlen (buf));
        }

        else printf ("Command not recognized\n");
    }
}

// User authentication and parse auth message
bool auth (char* msg, char* userBuf) {
    char* line = NULL;
    FILE* conf;
    ssize_t len = 0;
    ssize_t bytesRead;

    char* user = strchr (msg, ' ') + 1;
    char* pass = strchr (msg, ':') + 1;
    *(pass - 1) = '\0';

    strncpy (userBuf, user, BUF);
    printf ("Checking dfc.conf for user %s with password %s\n", user, pass);

    conf = fopen ("dfs.conf", "r");
    if (conf == NULL) {
        printf ("Failed to open dfs.conf\n");
        return false;
    }

    while ((bytesRead = getline (&line, &len, conf)) != -1) {
        line[strcspn (line, "\n")] = 0;
        char* confPass = strchr (line, ' ') + 1;
        *(confPass - 1) = '\0';
        if (!strcmp (pass, confPass) && !strcmp (user, line)) {
            printf ("User %s found, authentication complete\n", line);
            fclose (conf);
            return true;
        }
    }
    printf ("User %s not found\n", user);
    fclose (conf);
    return false;
}

// initiate user's directory on server if not already
void createUserDir (char* user) {
    DIR* d;
    char dirpath[strlen (path) + strlen (user) + 3];
    memset (dirpath, 0, sizeof (dirpath));

    strcpy (dirpath, path + 1);
    strcat (dirpath, "/");
    strcat (dirpath, user);

    printf ("Checking for %s\n", dirpath);

    d = opendir (dirpath);
    if (d) {
        printf ("Directory already exists\n");
        closedir (d);
    }
    else if (errno == ENOENT) {
        printf ("Directory not found, creating new...\n");
        if (mkdir (dirpath, 0777) != 0) printf ("Error in mkdir: errno %d\n", errno);
    }
    else {
        printf ("Failed to open directory\n");
    }
}


/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd (int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt (listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*) &optval, sizeof (int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port on any IP address for this host */
    bzero ((char*) &serveraddr, sizeof (serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
    serveraddr.sin_port = htons ((unsigned short) port);
    if (bind (listenfd, (struct sockaddr*) &serveraddr, sizeof (serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen (listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */