/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

/*
 * New wrapper functions, return 0 and print error message rather than terminate the process on failure, and return the number of bytes read on success
 */
ssize_t Rio_readn_w(int fd, void* usrbuf, size_t n)
{
    ssize_t readn;
    if(0 > (readn = rio_readn(fd, usrbuf, n)))
    {
        readn = 0;
        fprintf(stderr, "Rio_readn error");
    }
    return readn;
}
ssize_t Rio_readnb_w(rio_t* rp, void* usrbuf, size_t n)
{
    ssize_t readnb;
    if(0 > (readnb = rio_readnb(rp, usrbuf, n)))
    {
        readnb = 0;
        fprintf(stderr, "Rio_readnb error");
    }
    return readnb;
}
ssize_t Rio_readlineb_w(rio_t* rp, void* usrbuf, size_t maxlen)
{
   ssize_t readlineb;
    if(0 > (readlineb = rio_readlineb(rp, usrbuf, maxlen)))
    {
        readlineb = 0;
        fprintf(stderr, "Rio_readlineb error");
    }
    return readlineb;
}
void Rio_writen_w(int fd, void* usrbuf, size_t n)
{
    if(n != rio_writen(fd, usrbuf, n))
    {
        fprintf(stderr, "Rio_writen error");
    }
}

/*
 * Self-defined structure used to wrap the arguments of the thread routine
 * used to prevent data race
 */
typedef struct doitargs
{
    int tid;
    int connfd;
    struct sockaddr_in clientaddr;
} doitargs_t;

/* Semaphores used to protect log file*/
sem_t logMutex;

/*
 * doit - the thread routine used to process a request
 */
void doit(doitargs_t* argsptr)
{
    // variables used
    int threadid;                                               // a small integer ID of this thread
    struct sockaddr_in clientaddr;                              // client address
    int connfd;                                                 // the file descriptor used when connect to client as a server
    size_t n;                                                   // number of bytes transferred per read
    char buf[MAXLINE], requestLine[MAXLINE];                    // the read buffer and the requestline to forward to the server
    rio_t clientSideRio, serverSideRio;                         // rio buffers used to read request from client and write them to server
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];       // three parts of request header
    char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];   // the result of parse_uri
    int clientfd;                                               // the file descriptor used when connect to server as a client
    int receiveSize = 0, contentLength = 0;                     // the size in bytes of the object that was returned and the length of the request/response body
    char logstring[MAXLINE];                                    // the generated log

    // Get arguments and free the argument pointer
    threadid = argsptr->tid;
    clientaddr = argsptr->clientaddr;
    connfd = argsptr->connfd;
    Free(argsptr);

    // Get server hostname and port number from request line
    // Close all open socket descriptors and free all memory resources whenever the process fails to prevent memory leak
    Rio_readinitb(&clientSideRio, connfd);
    if(0 == Rio_readlineb_w(&clientSideRio, buf, MAXLINE))
    {
        fprintf(stderr, "cannot get request line\n");
        Close(connfd);
        return;
    }
    if(3 != sscanf(buf, "%s %s %s", method, uri, version))
    {
        fprintf(stderr, "incorrect request line format\n");
        Close(connfd);
        return;
    }
    if(0 > parse_uri(uri, hostname, pathname, port))
    {
        fprintf(stderr, "parse_uri error\n");
        Close(connfd);
        return;
    }
 
    // Connect to the required server and forward the request
    if(-1 == (clientfd = open_clientfd(hostname, port)))
    {
        fprintf(stderr, "cannot connect to server");
        Close(connfd);
        return;
    }
    Rio_readinitb(&serverSideRio, clientfd);
    // request line
    sprintf(requestLine, "%s%s%s%s%s%s", method, " /", pathname, " ", version, "\r\n");
    Rio_writen_w(clientfd, requestLine, strlen(requestLine));
    // request header
    while(0 != (n = Rio_readlineb_w(&clientSideRio, buf, MAXLINE)))
    {
        Rio_writen_w(clientfd, buf, n);
        // the end of request header
        if(!strcmp("\r\n", buf))
        {
            break;
        }
        // get the content length if exists
        if(!strncmp("Content-Length: ", buf, 16))
        {
            sscanf(buf+16, "%d", &contentLength);
        }
    }
    // request body 
    if(strcmp("GET", method))
    {
        // the body ends with 0\r\n if it is chunked
        if(0 == contentLength)
        {
            while(0 != (n = Rio_readlineb_w(&clientSideRio, buf, MAXLINE)))
            {
                Rio_writen_w(clientfd, buf, n);
                if(!strcmp(buf, "0\r\n"))
                {
                    break;
                }
            }
        }
        // or we can use content length to determine how many bytes to read
        else
        {
            for(int i = 0; i < contentLength; i++)
            {
                if(0 < Rio_readnb_w(&clientSideRio, buf, 1))
                {
                    Rio_writen_w(clientfd, buf, 1);
                }
            }
        }
    }
    contentLength = 0;              // set contentLength to zero to reuse it while processing response

    // Get response from server and forward it to the client
    // response header
    while(0 != (n = Rio_readlineb_w(&serverSideRio, buf, MAXLINE)) && strcmp("\r\n", buf))
    {
        // forward the response and record the length of the current line
        Rio_writen_w(connfd, buf, n);
        receiveSize += n;
        // get the content length if exists
        if(!strncmp("Content-Length: ", buf, 16))
        {
            sscanf(buf+16, "%d", &contentLength);
        }
    }
    // add the empty line
    Rio_writen_w(connfd, "\r\n", 2);
    receiveSize += 2;
    // response body
    if(0 == contentLength)          // the body ends with 0\r\n if it is chunked
    {
        while(0 != (n = Rio_readlineb_w(&serverSideRio, buf, MAXLINE)))
        {
            Rio_writen_w(connfd, buf, n);
            receiveSize += n;
            if(!strcmp("0\r\n", buf))
            {
                break;
            }
        }
    }
    else                            // or we can use content length to determine how many bytes to read
    {
        for(int i = 0; i < contentLength; i++)
        {
            if(0 < Rio_readnb_w(&serverSideRio, buf, 1))
            {
                Rio_writen_w(connfd, buf, 1);
                receiveSize += 1;
            }
        }
    }

    // write log into logfile
    // use semaphore to protect access to log file
    P(&logMutex);
    FILE* fp = fopen("proxy.log", "a");
    format_log_entry(logstring, &clientaddr, uri, receiveSize);
    printf("%s\n", logstring);
    fclose(fp);
    V(&logMutex);

    Close(connfd);
    Close(clientfd);
    return;
}

/* Thread routine */
void* thread(void* vargp)
{
    Pthread_detach(Pthread_self());         // detach itself to prevent memory leak
    doitargs_t* argsptr = (doitargs_t*)vargp;
    doit(argsptr);
    return NULL;
}

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    // variables used to receive request
    int listenfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    doitargs_t* doitargsptr;
    pthread_t tid;
    int threadid = 0;

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    Sem_init(&logMutex, 0, 1);
    listenfd = Open_listenfd(argv[1]);
    Signal(SIGPIPE, SIG_IGN);
    while(1)
    {
        // dynamically allocate a block for arguments of each thread to prevent data race
        doitargsptr = Malloc(sizeof(doitargs_t));
        doitargsptr->tid = threadid;
        doitargsptr->connfd = Accept(listenfd, (SA *)(&(doitargsptr->clientaddr)), &clientlen);
        Pthread_create(&tid, NULL, thread, doitargsptr);
        threadid++;
    }

    Close(listenfd);
    exit(0);
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}


