/* 
 ____                        ____
|  _ \ _ __ _____  ___   _  / ___|  ___ _ ____   _____ _ __
| |_) | '__/ _ \ \/ / | | | \___ \ / _ \ '__\ \ / / _ \ '__|
|  __/| | | (_) >  <| |_| |  ___) |  __/ |   \ V /  __/ |
|_|   |_|  \___/_/\_\\__, | |____/ \___|_|    \_/ \___|_|
                     |___/
Ryan Taylor
CSCI 4273 : Network Systems
Fall 2021
Programming Assignment 3
*/

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
#include <errno.h>
#include <openssl/md5.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

char cacheDNS[MAXLINE];
pthread_mutex_t dns_lock;
pthread_mutex_t cache_lock;
int timeout;


// defined helper functions
void send_error(int connfd, char *msg);
int checkBlacklist(char *hostname, char *ip);
int checkDNSCache(char *hostname, struct in_addr *cache_addr);
int addIPToCache(char *hostname, char *ip);
void md5_hash(char *str, char *md5buf);
int checkMD5Cache(char *fname);
void sendFromCache(int connfd, char *fname);

void *thread(void *vargp);
void echo(int connfd);
int open_listenfd(int port);



int main(int argc, char **argv) {
  int listenfd, *connfdp, port, clientlen = sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  pthread_t tid;

  memset(cacheDNS, 0, sizeof(cacheDNS));

  // no tiimeout provided
  if (argc == 2) timeout = 0;
  else if (argc == 3) {
    timeout = atoi(argv[2]);
    printf("Timeout set to %d seconds.\n", timeout);
  } else {
    fprintf(stderr, "usage: %s <port>\nor: %s <port> <timeout>\n", argv[0], argv[0]);
    exit(0);
  }

  // socket on given port
  port = atoi(argv[1]);
  listenfd = open_listenfd(port);

  // dns mutex lock
  if (pthread_mutex_init(&dns_lock, NULL) != 0) {
    printf("Cannot init mutex\n");
    return -1;
  }
  // cache mutex lock
  if (pthread_mutex_init(&cache_lock, NULL) != 0) {
    printf("Cannot init mutex\n");
    return -1;
  }

  while (1) {
    connfdp = malloc(sizeof(int));
    *connfdp = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    pthread_create(&tid, NULL, thread, connfdp);
  }

  printf("Broken out of while loop, quitting.\n");
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

/* Sends specified error message back to server*/
void send_error(int connfd, char *msg) {
  char errormsg[MAXLINE];
  sprintf(errormsg, "HTTP/1.1 %s\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n", msg);
  write(connfd, errormsg, strlen(errormsg));
}

// check blacklisted hosts / ips
int checkBlacklist(char *hostname, char *ip) {
  FILE *fp;
  char line[100];
  char *newline;

  // no blacklist file
  if (access("blacklist", F_OK) == -1) {
    printf("No blacklist found\n");
    return 0;
  }

  fp = fopen("blacklist", "r");

  while (fgets(line, sizeof(line), fp)) {
    newline = strchr(line, '\n');

    if (newline != NULL)
      *newline = '\0';

    if (strstr(line, hostname) || strstr(line, ip)) {
      printf("Blacklist match found: %s\n", line);
      return 1;
    }
  }

  fclose(fp);
  printf("No blacklist match found\n");
  return 0;
}

/* Checks dns cache to see if hostname has been resolved before */
int checkDNSCache(char *hostname, struct in_addr *cache_addr) {
  printf("Checking for %s in cache!!!\n", hostname);

  char *line;
  char *tmpbuf = calloc(strlen(cacheDNS) + 1, sizeof(char));
  strcpy(tmpbuf, cacheDNS); //Using strtok modifies original string, make a temp copy

  char *match = strstr(tmpbuf, hostname);

  if (match == NULL) // Hostname not in cache
    return -1;

  line = strtok(match, ":");
  line = strtok(NULL, "\n");
  printf("Found DNS cache entry %s:%s\n", hostname, line);
  inet_pton(AF_INET, line, cache_addr);
  free(tmpbuf);
}

// add hostname and ip to cache
int addIPToCache(char *hostname, char *ip) {

  pthread_mutex_lock(&dns_lock);
  char *entry = strrchr(cacheDNS, '\n');
  char buf[100];
  memset(buf, 0, sizeof(buf));
  snprintf(buf, 100, "%s:%s\n", hostname, ip);

  if (entry == NULL) {
    printf("Cache empty\n");
    strncpy(cacheDNS, buf, strlen(buf));
    pthread_mutex_unlock(&dns_lock);
    return 0;
  }

  if (entry + strlen(buf) + 1 > cacheDNS + sizeof(cacheDNS)) { // Cache full
    return -1;
    pthread_mutex_unlock(&dns_lock);
  }

  strncpy(entry + 1, buf, strlen(buf));
  pthread_mutex_unlock(&dns_lock);
}

// Old md5 hash implementation I already had
void md5_hash(char *str, char *md5buf) {
  unsigned char md5sum[16];
  MD5_CTX context;
  MD5_Init(&context);
  MD5_Update(&context, str, strlen(str));
  MD5_Final(md5sum, &context);

  for (int i = 0; i < 16; ++i)
    sprintf(md5buf + i * 2, "%02x", (unsigned int)md5sum[i]);

  printf("Final hash: %s -> %s\n", str, md5buf);
}

/*
Checks local cache based on md5 hash of file name
returns 0 if not in cache or in cache but too old based on timeout, and 1 if in cache within timeout
*/
int checkMD5Cache(char *fname) {
  struct stat file_stat;
  DIR *dir = opendir("./cache");

  //sizeof("cache") includes null terminator, but it will be replace with '/'
  char buf[strlen("cache/") + strlen(fname)];
  memset(buf, 0, sizeof(buf));
  strcpy(buf, "cache/");
  strcat(buf, fname);
  printf("Searching for file %s\n", buf);

  if (dir) {
    closedir(dir);

    if (stat(buf, &file_stat) != 0) {
      printf("File not in cache\n");
      return 0;
    }

    printf("File found in cache, checking timeout...\n");

    if (timeout == 0) {
      printf("No timeout value set, assuming file is valid\n");
      return 1;
    }

    time_t file_modify = file_stat.st_mtime;
    time_t current_time = time(NULL);
    double diff = difftime(current_time, file_modify);

    if (diff > timeout) {
      printf("Timeout occurred: file was modified %.2f seconds ago, timeout is %d\n", diff, timeout);
      return 0;
    }

    printf("File is valid for %d more seconds\n", timeout - (int)diff);
    return 1;
  } else if (errno = ENOENT) {
    printf("Cache folder does not exist, creating...\n");
    mkdir("cache", 0777);
    printf("Created directory\n");
    closedir(dir);
    return 0;
  } else {
    printf("Error in opening cache folder\n");
    return 0;
  }
}

/* Sends file to client from local cache */
void sendFromCache(int connfd, char *fname) {
  FILE *f = fopen(fname, "rb");

  if (!f) {
    printf("Error opening file %s\n", fname);
    return;
  }

  fseek(f, 0L, SEEK_END);
  int fsize = ftell(f);
  rewind(f);
  char file_buf[fsize];
  fread(file_buf, 1, fsize, f);
  write(connfd, file_buf, fsize);
}

void echo(int connfd) {
  size_t n;
  char buf[MAXLINE];
  struct sockaddr_in serveraddr;
  struct hostent *server;
  int sockfd;
  int size;
  int portno = 80;

  n = read(connfd, buf, MAXLINE);
  printf("\nServer received a request\n");

  char *request = strtok(buf, " "); // GET or POST
  char *hostname = strtok(NULL, " ");
  char *version = strtok(NULL, "\r"); // HTTP/1.1 or HTTP/1.0
  char fname[MAXLINE];

  printf("Request type: %s\nHostname: %s\nVersion: %s\n", request, hostname, version);

  // null hostname
  if (hostname == NULL || version == NULL) {
    printf("Hostname or version is NULL\n");
    send_error(connfd, "500 Internal Server Error");
    return;
  }

  // no hostname
  if (strlen(hostname) == 0) {
    printf("No host requested, responding with error\n");
    send_error(connfd, "500 Internal Server Error");
    return;
  }

  // invalid http
  if (!strcmp(version, "HTTP/1.1") || !strcmp(version, "HTTP/1.0")) {
  } else {
    printf("Invalid HTTP version: %s\nResponding with error\n", version);
    send_error(connfd, "500 Internal Server Error");
    return;
  }

  // any request type except get
  if (!strcmp(request, "GET")) {
  } else {
    printf("Invalid HTTP request: %s\nResponding with 400 error\n", request);
    send_error(connfd, "400 Bad Request");
    return;
  }

  // clean up string to process hostname correctly
  char *doubleSlash = strstr(hostname, "//");
  if (doubleSlash != NULL) {
    hostname = doubleSlash + 2;
  }

  // Grab default page if requested
  char *slash = strchr(hostname, '/');
  if (slash == NULL || *(slash + 1) == '\0') {
    printf("Default page requested\n");
    strcpy(fname, "index.html");
  } else strcpy(fname, slash + 1);

  // confirm host and file
  printf("Host: %s\nFile: %s\n", hostname, fname);

  // clean string for DNS processing
  if (slash != NULL) *slash = '\0';

  char md5_input[strlen(hostname) + strlen(fname) + 2]; //2 extra bytes for slash between hostname and filename, and null terminator
  strcpy(md5_input, hostname);
  strcat(md5_input, "/");
  strcat(md5_input, fname);

  // Hash and cache hostname
  char md5_output[33];
  memset(md5_output, 0, sizeof(md5_output));
  printf("Hashing %s\n", md5_input);
  md5_hash(md5_input, md5_output);
  char cache_buf[strlen("cache/") + strlen(md5_output)];
  strcpy(cache_buf, "cache/");
  strcat(cache_buf, md5_output);

  // if cached, send cache output
  if (checkMD5Cache(md5_output)) {
    printf("Sending file %s\n", cache_buf);
    sendFromCache(connfd, cache_buf);
    return;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
    printf("ERROR opening socket");

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(portno);


  struct in_addr cache_addr;
  int serveraddr_cache = checkDNSCache(hostname, &cache_addr);

  if (serveraddr_cache == -1) { // not in cache
    printf("Host %s not in DNS cache\n", hostname);
    server = gethostbyname(hostname);

    if (server == NULL) {
      printf("Unable to resolve host %s, responding with 404 error\n", hostname);
      send_error(connfd, "404 Not Found");
      return;
    }

    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  } else   //in cache
    serveraddr.sin_addr.s_addr = cache_addr.s_addr;


  char IPbuf[20];

  if (inet_ntop(AF_INET, (char *)&serveraddr.sin_addr.s_addr, IPbuf, (socklen_t)20) == NULL) {
    printf("ERROR in converting hostname to IP\n");
    return;
  }

  // add to cache if not already there
  if (serveraddr_cache == -1) {
    
    int success = addIPToCache(hostname, IPbuf);

    // error adding file to cache
    if (success == -1) printf("Cache full, cannot add entry %s:%s\n", hostname, IPbuf);
  }

  printf("Host: %s, IP: %s\nChecking blacklist\n", hostname, IPbuf);

  // if blacklisted 
  if (checkBlacklist(hostname, IPbuf)) {
    send_error(connfd, "403 Forbidden");
    return;
  }


  int serverlen = sizeof(serveraddr);
  size = connect(sockfd, (struct sockaddr *)&serveraddr, serverlen);
  if (size < 0) { // error catching
    printf("ERROR in connect\n");
    return;
  }

  memset(buf, 0, MAXLINE);
  sprintf(buf, "GET /%s %s\r\nHost: %s\r\n\r\n", fname, version, hostname);

  size = write(sockfd, buf, sizeof(buf));
  if (size < 0) { // error catching
    printf("ERROR in sendto\n");
    return;
  }

  int total_size = 0;
  memset(buf, 0, sizeof(buf));

  FILE *fp;
  fp = fopen(cache_buf, "wb");
  pthread_mutex_lock(&cache_lock);

  while ((size = read(sockfd, buf, sizeof(buf))) > 0) {
    if (size < 0) { // error catching
      printf("ERROR in recvfrom\n");
      return;
    }

    total_size += size;

    write(connfd, buf, size);
    fwrite(buf, 1, size, fp);
    memset(buf, 0, sizeof(buf));
  }

  pthread_mutex_unlock(&cache_lock);
  fclose(fp);

  if (size == -1) { // error catching
    printf("Error in read - errno:%d\n", errno);
    return;
  }

  printf("received a total of %d bytes\n", total_size);
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
