#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 10
#define SBUFSIZE 20
#define LBUFSIZE 20

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

#define __MAC_OS_X
#include "csapp.h"
#include "sbuf.h"

typedef struct 
{
	char *header;
	char *value;
} HeaderPair;

void doit(int fd);
int read_requesthdrs(rio_t * rp, HeaderPair headers[]);
int	parse_uri(char *uri, char *filename, char *domain, char *portNum);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
	    char *shortmsg, char *longmsg);
int indexOf(const char *str, const char toFind);
int constructFirstLine(char *request, char *filename);
sbuf_t sbuf;
logBuf_t lbuf;
Cache cache;

	    
void *thread(void *vargp)
{
    Pthread_detach(pthread_self());
    while (1) 
    {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
        doit(connfd);                	/* Service client */
        Close(connfd);
    }
}

void *lbuf_run(void *vargp)
{
	while(1)
	{
		lbuf_remove(&lbuf);
	}
}

int main(int argc, char **argv)
{
    int i, listenfd, connfd;
    int msglen = 0;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
 
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    lbuf_init(&lbuf, LBUFSIZE);
    cacheInit(&cache, MAX_OBJECT_SIZE);
    Pthread_create(&tid, NULL, lbuf_run, NULL);
    char *msg = "Spinning up threadpool";
    msglen = strlen(msg);
    char *startmsg = calloc(msglen, sizeof(char));
    strncpy(startmsg, msg, msglen);
    lbuf_insert(&lbuf, startmsg);
    
    for (i = 0; i < NTHREADS; i++) 
    { /* Create worker threads */ 	
		Pthread_create(&tid, NULL, thread, NULL);
	}               
    while (1) 
    {
		clientlen = sizeof(struct sockaddr_storage);
		connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
		char* msg = "sending connection to a thread\0";
		msglen = strlen(msg);
		char* logmsg = calloc(msglen, sizeof(char));
		strncpy(logmsg, msg, msglen);
		lbuf_insert(&lbuf, logmsg);
    }
}
/* $end main */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
	printf("beginning doit\n");
	int client_fd = 0;
	int msglen = 0;
	int numHeaders = 0;
	int reqLen = 0;
	int tempLen = 0;
	int recvLength = 0;
	int done = 0;
	int recvHold = 0;
	char portNum[DEFAULTPORT] = "80";
	struct stat	sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE] = "/", domain[MAXLINE];
	char request[MAXLINE], *response = malloc(sizeof(char) * MAX_OBJECT_SIZE);
	HeaderPair headers[HEADER_LIST_SIZE];
	rio_t rio;

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	if (!Rio_readlineb(&rio, buf, MAXLINE))
	{
		return;
	}
	//printf("%s\n", buf);
	sscanf(buf, "%s %s %s", method, uri, version);
	//check for GET method
	if (strcasecmp(method, "GET"))
	{
		//If not a GET request, spit error back
		clienterror(fd, method, "501", "Not Implemented",
				"Proxy only implements GET");
		return;
	}
	msglen = strlen(uri);
	char* logmsg = calloc(msglen, sizeof(char));
	strncpy(logmsg, uri, msglen);
	lbuf_insert(&lbuf, logmsg);
	//Read the headers into our array we created above
	//numHeaders will hold the current size
	numHeaders = read_requesthdrs(&rio, headers);
	//Printing of headers for error checking
	//printf("%d", numHeaders);
	for (int i = 0; i < numHeaders; i++)
	{
		int checker = 0;
		if(strcmp(headers[i].header, "Host") == 0)
		{
			checker = indexOf(headers[i].value, ':');
			if(checker > 0)
			{
				headers[i].value[checker] = '\r';
				headers[i].value[checker+1] = '\0';
			}
		}
		//printf("%s: %s\n", headers[i].header, headers[i].value);
	}
	/* Parse URI from GET request into filename, domain, and portnumber*/
	parse_uri(uri, filename, domain, portNum);
	//printf("%s", filename);
	//Creating the addrinfo we will use to open a socket to the 
	//machine we are attempting to connect to
	client_fd = open_clientfd(domain, portNum);
	reqLen = constructFirstLine(request, filename);
	//Copy headers into the request
	for(int i = 0; i < numHeaders; i++)
	{
		tempLen = strlen(headers[i].header);
		strncpy(&request[reqLen], headers[i].header, tempLen);
		reqLen += tempLen;
		request[reqLen] = ':';
		reqLen++;
		tempLen = strlen(headers[i].value);
		strncpy(&request[reqLen], headers[i].value, tempLen);
		reqLen += tempLen;
		request[reqLen] = '\n';
		reqLen++;
	}
	request[reqLen] = '\r';
	reqLen++;
	request[reqLen] = '\n';
	reqLen++;
	
	send(client_fd, request, reqLen, 0);
	
	while(recvLength < MAX_OBJECT_SIZE && done != 1)
	{
		recvHold = recv(client_fd, (response+recvLength), (MAX_OBJECT_SIZE-recvLength), MSG_WAITALL);
		if(recvHold > 0)
		{
			recvLength += recvHold;
		} 
		else
		{
			done = 1;
		}
	}
	rio_writen(fd, response, recvLength);
	for (int i = 0; i < numHeaders; i++)
	{
		free(headers[i].header);
		free(headers[i].value);
	}
	//Close(client_fd);
	free(response);
	return;
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp, HeaderPair headers[])
{
	int count = 0;
	int length = 0;
	int hasConnection = 0;
	int hasHost = 0;
	int hasProxyCon = 0;
	char buf [MAXLINE];
	char colon = ':';
	char newline = '\n';
	char *saveptr;
	char *holder;
	
	Rio_readlineb(rp, buf, MAXLINE);
	//While there is still a header to get
	while (strcmp(buf, "\r\n"))
	{
		headers[count].header = malloc(sizeof(char) * HEADER_LIST_SIZE);
		headers[count].value = malloc(sizeof(char) * HEADER_LIST_SIZE);
		//line: netp: readhdrs:checkterm
		holder = strtok_r(buf, &colon, &saveptr);
		length = strlen(holder);
		strncpy(headers[count].header, holder, length);
		//headers[count].header = strtok_r(buf, &colon, &saveptr);
		holder = strtok_r(NULL, &newline, &saveptr);
		length = strlen(holder);
		strncpy(headers[count].value, holder, length);
		//If there is a connection or proxy connection header, set the value to close
		if((!strcmp(headers[count].header, "Connection") 
		|| !strcmp(headers[count].header, "Proxy-Connection")))
		{
			strcpy(headers[count].value, "Close\r");
		}
		//headers[count].value = strtok_r (NULL, &newline, &saveptr);
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
		count++;
	}
	//Find if there was a Connection, Proxy Connection, and Host header
	for(int i = 0; i < count; i++)
	{
		if(strcmp(headers[i].header, "Proxy-Connection") == 0)
		{
			hasProxyCon = 1;
		}
		else if(strcmp(headers[i].header, "Connection") == 0)
		{
			hasConnection = 1;
		}
		else if(strcmp(headers[i].header, "Host") == 0)
		{
			hasHost = 1;
		}
	}
	//If any of those above headers are not present, create them and set
	//its value properly
	if(hasProxyCon == 0)
	{
		headers[count].header = malloc(sizeof(char) * HEADER_LIST_SIZE);
		headers[count].value = malloc(sizeof(char) * HEADER_LIST_SIZE);
		strcpy(headers[count].header, "Proxy-Connection");
		strcpy(headers[count].value, " Close\r");
		count++;
	}
	if(hasHost == 0)
	{
		headers[count].header = malloc(sizeof(char) * HEADER_LIST_SIZE);
		headers[count].value = malloc(sizeof(char) * HEADER_LIST_SIZE);
		strcpy(headers[count].header, "Host");
		count++;
	}
	if(hasConnection == 0)
	{
		headers[count].header = malloc(sizeof(char) * HEADER_LIST_SIZE);
		headers[count].value = malloc(sizeof(char) * HEADER_LIST_SIZE);
		strcpy(headers[count].header, "Connection");
		strcpy(headers[count].value, " Close\r");
		count++;
	}
	return count;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args return 0 if dynamic
 * content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *domain, char *portNum)
{
	char *holder = malloc(sizeof(char) * MAXLINE);
	int index = 0;
	int portCheck = 0;
	int fileCheck = 0;
	int domainLength = 0;
	//If it has the prefix, jump over it
	if (strstr(uri, "http://") || strstr(uri, "https://"))
	{	
		//find the end of "http/https://
		index = indexOf(uri, '/');
		//jump past it
		index += 2;
	}
		//copy what's left into our holder char *
		strcpy(holder, &uri[index]);
		//check if there's a port after the domain
		portCheck = indexOf(holder, ':');
		//check where the domain/port(if there is a port) ends
		fileCheck = indexOf(holder, '/');
		//check how long the total length is (to compare with fileCheck)
		//If there is a fileName after, domainLength > fileCheck
		domainLength = strlen(holder);
		//If we found a ':'
		if (portCheck != -1)
		{
			//Check whether it occurs before the final '/'
			if (portCheck < fileCheck)
			{
				//if it does, that means we have a port number given to us
				strncpy(portNum, &holder[portCheck+1], ((fileCheck-portCheck)-1));
			}
		}
		//Jump to the character beyond the '/'
		//If that spot is NOT the end of the string, get whatever is
		//after the '/' as the filename
		if (domainLength > fileCheck + 1)
		{
			strncpy(filename, &holder[fileCheck], domainLength-fileCheck);
			strncpy(domain, holder, portCheck);
		}
		//If there is no filename after the domain, simply copy the domain 
		else
		{
			strncpy(domain, holder, portCheck);
		}
		
		free(holder);
		return 1;
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	/* Send response headers to client */
	get_filetype(filename, filetype);
	//line: netp: servestatic:getfiletype
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	//line: netp: servestatic:beginserve
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	//line: netp: servestatic:endserve
	printf("Response headers:\n");
	printf("%s", buf);

	/* Send response body to client */
	srcfd = Open(filename, O_RDONLY, 0);
	//line: netp: servestatic:open
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	//line: netp: servestatic:mmap
	Close(srcfd);
	//line: netp: servestatic:close
	Rio_writen(fd, srcp, filesize);
	//line: netp: servestatic:write
	Munmap(srcp, filesize);
	//line: netp: servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf [MAXLINE], *emptylist[] = {NULL};

	/* Return first part of HTTP response */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	if (Fork() == 0)
	{			/* Child */
	//line: netp: servedynamic:fork
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1);
	//line: netp: servedynamic:setenv
		Dup2(fd, STDOUT_FILENO);	/* Redirect stdout to
							 * client */
	//line: netp: servedynamic:dup2
		Execve(filename, emptylist, environ);	/* Run CGI program */
	//line: netp: servedynamic:execve
	}
	Wait(NULL);		/* Parent waits for and reaps child */
//line: netp: servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
	    char *shortmsg, char *longmsg)
{
	char buf [MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

int indexOf(const char *str, const char toFind)
{
	int toRet = 0;
	while (str[toRet] != '\0')
	{
		if (str[toRet] == toFind)
		{
			return toRet;
		}
		toRet++;
	}
	return -1;
}

int constructFirstLine(char *request, char *filename)
{
	char *Http = "HTTP/1.0";
	char *method = "GET";
	int tempLen = 0;
	int reqLen = 0;
	tempLen = strlen(method);
	strncpy(request, method, tempLen);
	reqLen += tempLen;
	request[reqLen] = ' ';
	reqLen++;
	tempLen = strlen(filename);
	strncpy(&request[reqLen], filename, tempLen);
	reqLen += tempLen;
	request[reqLen] = ' ';
	reqLen++;
	tempLen = strlen(Http);
	strncpy(&request[reqLen], Http, tempLen);
	reqLen += tempLen;
	request[reqLen] = '\r';
	reqLen++;
	request[reqLen] = '\n';
	reqLen++;
	return reqLen;
}
/* $end clienterror */
