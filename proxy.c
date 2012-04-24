#include <stdio.h>
#include "csapp.h"
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_line = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

void doit(int fd, char *method, char *uri, char *version, char *host);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs, int *port);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
	char *shortmsg, char *longmsg);
void get_header_info(int fd, char *method, char *uri, 
	char *version, char *host);

int main(int argc, char **argv){
    printf("%s%s%sport:%s \n", user_agent, accept_line, accept_encoding, argv[1]);
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port = atoi(argv[1]);
    

    listenfd = Open_listenfd(port);
    while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		char host[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
		get_header_info(connfd, method, uri, version, host);
        doit(connfd, method, uri, version, host);
		Close(connfd);
    }
    return 0;
}

void get_header_info(int fd, char *method, char *uri, char *version, char *host){
    char buf[MAXLINE];
    rio_t rio;
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	//reads the first line to get request line
	sscanf(buf, "%s %s %s", method, uri, version);
    /* Read headers */
    while (1){
    	Rio_readinitb(&rio, fd);
	    Rio_readlineb(&rio, buf, MAXLINE);
	    if (!strcmp(buf,"\r\n")) break;
    	printf("reading line: %s",buf);
    	char key[MAXLINE], value[MAXLINE];
    	sscanf(buf, "%s %s", key, value);
    	// extract the host 
    	if (!strcmp(key, "Host:")){
    		strncpy(host, value, MAXLINE);
    		printf("  host: %s\n\n", host);
    	}
    }
}


/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd, char *method, char *uri, char *version, char *host) 
{
    int is_static;
    int port = 0;
    struct stat sbuf;
    char filename[MAXLINE], cgiargs[MAXLINE];

    if (strcasecmp(method, "GET")) {
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }                               
    //read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs, &port);       //line:netp:doit:staticcheck
    printf("%s\n", filename);
    assert(0);
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
    	clienterror(fd, filename, "404", "Not found",
    		    "Tiny couldn't find this file");
    	return;
    }                                                    //line:netp:doit:endnotfound

    if (is_static) { /* Serve static content */          
    	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
    	    clienterror(fd, filename, "403", "Forbidden",
    			"Tiny couldn't read the file");
    	    return;
    	}
        printf("sldfjsdlkfjdfsl\n");
    	serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
}

/*
 * read_requesthdrs - read and parse HTTP request headers
 */

void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 * return 0 if dynamic content, 1 if static
 */
int parse_uri(char *uri, char *filename, char *cgiargs, int *port) 
{
    char *ptr;
    int httplength = 7;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
    	strcpy(cgiargs, "");  
    	char hostname[MAXLINE+1];
        char filename[MAXLINE];
        strcpy(filename, ".");

        if (uri == strstr(uri, "http://")){
            //if http is at the start of the uri
            char *pathStart = index(&uri[httplength], '/');
            strncpy(filename, pathStart, MAXLINE);
            strncpy(hostname, &uri[httplength], pathStart-&uri[httplength]);
            strcat(hostname, ""); //to put null character at end
        }
        else {
            // if no http
            strncpy(hostname, "", 1);
            strncpy(filename, uri, MAXLINE);
        }
        if ((ptr = index(hostname, ':')) != NULL){
            //if there is a port specified
            char portString[100];
            strncpy(portString, ptr+1, 100);
            *port = atoi(portString);
            ptr[0] = '\0';
        }
    	 
        printf("hostname: %s, filename: %s, port: %d\n",hostname, filename, *port);
    	return 1;
    }
    else {  /* dynamic content */                        //line:netp:parseuri:isdynamic
    	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
    	if (ptr) {
    	    strcpy(cgiargs, ptr+1);
    	    *ptr = '\0';
    	}
    	else{ 
    	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
        }
    	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
    	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
    	return 0;
    }
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
    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);       
    Rio_writen(fd, srcp, filesize);  
    Munmap(srcp, filesize);          
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
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
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
/* $end clienterror */
