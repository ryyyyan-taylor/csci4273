//  ___  ___ _ ____   _____ _ __ ___
// / __|/ _ \ '__\ \ / / _ \ '__/ __|
// \__ \  __/ |   \ V /  __/ | | (__
// |___/\___|_|    \_/ \___|_|(_)___|
//
// based on provided echo server files


#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#define MAXLINE  8192  /* max text line length */
#define SHORTBUF  256  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */


// colored outputs for the server operations
#define MSGERRR "\x1b[31m"
#define MSGSUCC "\x1b[32m"
#define MSGNORM "\x1b[0m"
#define MSGTERM "\x1b[35m"
#define MSGWARN "\x1b[33m"


// opening function declarations
int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);

//
int checkValidURL(const char *urlarg);
int checkValidVER(const char *verarg);
const char *fnameExtension(const char *fname);


int main(int argc, char **argv) {
    int listenfd, *connfdp, port, clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);

    listenfd = open_listenfd(port);

    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* thread routine */
void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}


void echo(int connfd) {
    
    size_t n;

    char buf[MAXLINE];
    char metBuf[SHORTBUF];
    char urlBuf[MAXLINE];
    char dotURLbuf[MAXLINE];
    char verBuf[SHORTBUF];
    char ftype[SHORTBUF];
    char contentType[SHORTBUF];

    int error500 = 0;
    int validURL = 0;
    int validVER = 0;

    char httpmsggiven[] = "HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:32\r\n\r\n<html><h1>Hello CSCI4273 Course!</h1>";
    char error500msg[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n";

    n = read(connfd, buf, MAXLINE);
    printf(MSGSUCC "server received the following request:\n%s" MSGSUCC "\n", buf);

    printf(MSGWARN "PARSING" MSGNORM "\n");
    char *token1 = strtok(buf, " ");
    size_t tk1len = strlen(token1);
    strncpy(metBuf, token1, tk1len);

    char *token2 = strtok(NULL, " ");
    size_t tk2len = strlen(token2);
    strncpy(urlBuf, token2, tk2len);

    char *token3 = strtok(NULL, "\r\n");
    size_t tk3len = strlen(token3);
    strncpy(verBuf, token3, tk3len);

    bzero(buf, MAXLINE);

    printf(MSGSUCC "GETTING FILE" MSGNORM "\n");
    FILE *fp = NULL;

    if (checkValidVER(verBuf))
        validVER = 1;
    else {
        validVER = 0;
        error500 = 1;
    }

    if (checkValidURL(urlBuf))
        validURL = 1;
    else {
        validURL = 0;
        error500 = 1;
    }

    if (validURL && validVER) {
        printf(MSGTERM "VALID URL AND VERSION" MSGNORM "\n");
        strcat(dotURLbuf, ".");
        strcat(dotURLbuf, urlBuf);
        printf(MSGTERM "dotURLbuf: %s" MSGTERM "\n", dotURLbuf);

        if (!strcmp(dotURLbuf, "./")) {
            printf(MSGTERM "DEFAULT WEBPAGE" MSGNORM "\n");
            fp = fopen("index.html", "rb");

            printf(MSGSUCC "READING FILE" MSGNORM "\n");
            fseek(fp, 0L, SEEK_END);
            n = ftell(fp);
            rewind(fp);
            printf(MSGSUCC "FILE READ" MSGNORM "\n");

            strcpy(ftype, "html");
            printf(MSGTERM "ftype: %s" MSGNORM "\n", ftype);
        } else if (fp = fopen(dotURLbuf, "rb")) {
            printf(MSGSUCC "READING FILE" MSGNORM "\n");
            fseek(fp, 0L, SEEK_END);
            n = ftell(fp);
            rewind(fp);
            printf(MSGSUCC "FILE READ" MSGNORM "\n");
            strcpy(ftype, fnameExtension(urlBuf));
            printf(MSGTERM "ftype: %s" MSGNORM "\n", ftype);
        } else {
            printf(MSGERRR "FILE DOES NOT EXIST" MSGNORM "\n");
            error500 = 1;
        }

        // if no error
        if (error500 == 0) {
            // http
            if (!strcmp(ftype, "html")) strcpy(contentType, "text/html");
            //txt
            else if (!strcmp(ftype, "txt")) strcpy(contentType, "text/plain");
            // png
            else if (!strcmp(ftype, "png")) strcpy(contentType, "image/png");
            // gif
            else if (!strcmp(ftype, "gif")) strcpy(contentType, "image/gif");
            // jpf
            else if (!strcmp(ftype, "jpg")) strcpy(contentType, "image/jpg");
            // css
            else if (!strcmp(ftype, "css")) strcpy(contentType, "text/css");
            // javascript
            else if (!strcmp(ftype, "js")) strcpy(contentType, "application/java");
            // default
            else printf(MSGERRR "NOT A VALID FILETYPE" MSGNORM "\n");

            printf(MSGTERM "content requested: %s" MSGNORM "\n", contentType);

            char *filebuff = malloc(n);
            fread(filebuff, 1, n, fp);

            char tempbuff[MAXLINE];
            sprintf(tempbuff, "HTTP/1.1 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", contentType, n);

            char *httpmsg = malloc(n + strlen(tempbuff));
            printf(MSGTERM "httpmsg: %s" MSGNORM "\n", httpmsg);
            sprintf(httpmsg, "%s", tempbuff);
            memcpy(httpmsg + strlen(tempbuff), filebuff, n);
            n += strlen(tempbuff);

            //printf(MSGTERM"server returning a http message with the following content.\n%s" MSGNORM "\n", httpmsg);
            write(connfd, httpmsg, n);

        // if found error
        } else {
            printf(MSGERRR "SENDING ERROR MESSAGE" MSGNORM "\n");
            n = strlen(error500msg);
            write(connfd, error500msg, n);
        }

    // default for invalid url or ver
    } else {
        printf(MSGERRR "NOT VALID" MSGNORM "\n");
        printf(MSGERRR "SENDING ERROR MESSAGE" MSGNORM "\n");
        n = strlen(error500msg);
        write(connfd, error500msg, n);
    }

    // clear variables
    bzero(buf, MAXLINE);
    bzero(metBuf, SHORTBUF);
    bzero(urlBuf, MAXLINE);
    bzero(dotURLbuf, MAXLINE);
    bzero(verBuf, SHORTBUF);
    bzero(ftype, SHORTBUF);
    bzero(contentType, SHORTBUF);
}

// fix filenames where necessary
const char *fnameExtension(const char *fname) {
    const char *period = strrchr(fname, '.');

    if (!period || period == fname)
        return "";

    return period + 1;
}

int checkValidURL(const char *urlarg) {
    int len_urlarg = strlen(urlarg);

    if ((urlarg != NULL) && (urlarg[0] == '\0')) {
        printf("urlarg is empty\n");
        return 0;
    } else
        return 1;
}

int checkValidVER(const char *verarg) {
    
    if (strlen(verarg) == 0) return 0;

    if ((verarg != NULL) && (verarg[0] == '\0')) {
        printf("verarg is empty\n");
        return 0;
    }
    else if (strcmp(verarg, "HTTP/1.1") == 0 || strcmp(verarg, "HTTP/1.0") == 0)
        return 1;
    // default
    else return 0;
}

/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;

    return listenfd;
} /* end open_listenfd */