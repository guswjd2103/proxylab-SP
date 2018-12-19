#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void *thread(void *vargp);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *port, char *path);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
//void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


int main(int argc, char **argv)
{
    	int listenfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;


	if(argc!=2){
		fprintf(stderr, "usage : %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);
	while(1){
		clientlen=sizeof(struct sockaddr_storage);
		//connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		int *connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		//printf("%s", user_agent_hdr);
		Pthread_create(&tid, NULL, thread, connfdp);
		//doit(connfd);
		//Close(connfd);
	}
	exit(0);
	return 0;
}

void *thread(void *vargp){
	int connfd = *((int *)vargp);
	Pthread_detach(Pthread_self());
	Free(vargp);
	doit(connfd);
	Close(connfd);
	return NULL;
}

void doit(int fd)
{
	//int is_static;
	//struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], buf2[MAXLINE];
	char filename[MAXLINE], port[MAXLINE], path[MAXLINE];
	rio_t rio, rserver;
	int server;
	size_t  numofbyte;


	Rio_readinitb(&rio, fd);
        Rio_readlineb(&rio, buf, MAXLINE);
        printf("Request headers : \n");
        printf("%s", buf);
        sscanf(buf, "%s %s %s", method, uri, version);
        if(strcasecmp(method, "GET")){
                clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
                return;
        }
        read_requesthdrs(&rio);
        parse_uri(uri, filename, port, path);
	
	//printf("host : %s\r\n\r\n", filename);
	//printf("port : %s\r\n\r\n", port);
	

	server = Open_clientfd(filename, port);
	Rio_readinitb(&rserver, server);
	sprintf(buf2, "GET %s HTTP/1.0\r\n", path);
	//printf("1 : %s", buf2);
	Rio_writen(server, buf2, strlen(buf2));

	sprintf(buf2, "Host: %s\r\n", filename);
	//printf("2 : %s", buf2);
	Rio_writen(server, buf2, strlen(buf2));
	
	sprintf(buf2, "User-Agent: %s\r\n",  "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n");
	//printf("3 : %s", buf2);
	Rio_writen(server, buf2, strlen(buf2));
	
	sprintf(buf2, "Connection: %s\r\n", "close");
	//printf("4 : %s", buf2);
	Rio_writen(server, buf2, strlen(buf2));
	
	sprintf(buf2, "Proxy-Connection: %s\r\n\r\n", "close");
	//printf("5 : %s", buf2);
	Rio_writen(server, buf2, strlen(buf2));

	while((numofbyte = Rio_readlineb(&rserver, buf2, MAXLINE))!='\0')
		Rio_writen(fd, buf2, numofbyte);

	/*printf("uri = %s\n", uri);*/
	/*is_static = parse_uri(uri, filename, cgiargs);
	if(stat(filename, &sbuf)<0){
		clienterror(fd, filename, "404", "Not found", "Tiny could't find this file");
		return;
	}
	
	if(is_static){
		if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR &sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}*/
	/*else{
		if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}*/
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];
	
	sprintf(body, "<html><title>Tiny Eroor</title>");
	sprintf(body, "%s<body bgcolor = ""ffffff"">\r\n", body);
	sprintf(body, "%s%s : %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
	
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type : text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length : %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
	char buf[MAXLINE];
	
	Rio_readlineb(rp, buf, MAXLINE);
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

int parse_uri(char *uri, char *filename, char *port, char *path)
{
	/*char *ptr;
	if(!strstr(uri, "cgi-bin")){
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if(uri[strlen(uri)-1]=='/')
			strcat(filename, "home.html");
		return 1;
	}*/
	/*else{
		ptr = index(uri, '?');
		if(ptr){
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		}
		else
			strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcpy(filename, uri);
		return 0;
	}*/
	char *end;
	if(strstr(uri, "http://")!=uri){
		exit(0);
	}
	uri=uri+7;
	if(strstr(uri, ":")==NULL){//when port is existed in uri
		strcpy(port, "80");
		end = strpbrk(uri, "/\r\n\0");
		strncpy(filename, uri, (end-uri));
        	
		char *temp = end;
		while(*temp!='\0'){
			temp++;
		}
		strncpy(path, end, (temp-end));
			
	}	
	else{//when port is not existed in uri
		end = strpbrk(uri, ":/\r\n\0");
		strncpy(filename, uri, (end-uri));
		
		char *temp = end;
		while(*temp!='/'){
			temp++;
		}
		strncpy(port, end+1, (temp-end-1));
		char *ttemp = temp;
		while(*ttemp !='\0'){
			ttemp++;
		}
		strncpy(path, temp, (ttemp-temp));
	}
	return 0;	 
}

void serve_static(int fd, char *filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];
	
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection : close\r\n", buf);
	sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
	if(strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if(strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if(strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else if(strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}


/*void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = {NULL};
	sprintf(buf, "HTTP/1.0 200 OK \r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server : Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));
	
	if(Fork()==0){
		setenv("QUERY_STRING", cgiargs, 1);
		Dup2(fd, STDOUT_FILENO);
		Execve(filename, emptylist, environ);
	}
	Wait(NULL);
}*/

