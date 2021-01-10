//Willis A. Hershey
//Source code for Cynthia the Linux HTTP server

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "status.h"
#include "../pthread_store/pthread_store.h"

//The following three defines represent the three arguments of the UNIX-specific socket() call
#define DOMAIN		AF_INET
#define TYPE		SOCK_STREAM
#define PROTOCOL	0

//The port at which we intend to listen for incoming traffic
#define SERVER_PORT	80

//This is the backlog-length parameter passed to listen()
#define BACKLOG		16

//This is the size of the buffer we read into for the read() calls
#define BUF_SIZE	(sysconf(_SC_PAGE_SIZE)-sizeof(threadData))

#define HTTP_VERSION		"HTTP/2.0"

static const char *requests[]={"GET","HEAD","POST","PUT","DELETE","TRACE","OPTIONS","CONNECT","PATCH"};

typedef struct{
  int fd;
  char buf[];
}threadData;

int parseRequest(char *buf){
  for(int c=0;c<sizeof requests/sizeof(char*);++c)
	if(!memcmp(buf,requests[c],strlen(requests[c])))
		return c;
  return -1;
}

void pipeHandler(int signal){
  threadData *data=pthread_discard();
  printf("Signal handler ");
  if(!data){
	printf("no storage\n");
	pthread_exit(0);
  }
  printf("storage found\n");
  close(data->fd);
  munmap(data,sysconf(_SC_PAGE_SIZE));
  pthread_exit(0);
}

void* HTTPthreadServe(void *data){
  pthread_store(data);
  threadData *d=data;
  pthread_detach(pthread_self());
  while(1){
	ssize_t bytes=read(d->fd,d->buf,BUF_SIZE-1);
	if(bytes==-1){
		perror("Read()");
		goto close;
	}
	if(!bytes)
		goto close;
	int request=parseRequest(d->buf);
	if(request==-1){
		sprintf(d->buf,"%s %s\r\n\r\n",STATUS_400,HTTP_VERSION);
		if(write(d->fd,d->buf,strlen(d->buf))==-1){
			perror("Write()");
			goto close;
		}
		else
			continue;
	}
	switch(request){
		case 0:
			printf("GET\n");
			break;
		case 1:
			printf("HEAD\n");
			break;
		case 2:
			printf("POST\n");
			break;
		case 3:
			printf("PUT\n");
			break;
		case 4:
			printf("DELETE\n");
			break;
		case 5:
			printf("TRACE\n");
			break;
		case 6:
			printf("OPTIONS\n");
			break;
		case 7:
			printf("CONNECT\n");
			break;
		case 8:
			printf("PATCH\n");
			break;
		default:
			printf("WHAT?\n");
			break;
	}
  }
close:
  printf("Close ");
  if(pthread_discard()){
	printf("storage found\n");
	close(d->fd);
	munmap(data,sysconf(_SC_PAGE_SIZE));
  }
  else
	printf("no storage\n");
  pthread_exit(0);
  return NULL;
}

int serve(int port,void*(threadFunc)(void*)){
  //First we create a socket for inet stream communication
  int socketfd=socket(DOMAIN,TYPE,PROTOCOL);
  struct sockaddr_in server,client;
  socklen_t length=(socklen_t)sizeof server;
  if(socketfd==-1){
	perror("socket()");
	exit(EXIT_FAILURE);
  }
  else
	printf("Socket created with file descriptor %d\n",socketfd);
  server.sin_family=DOMAIN;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(port);

  //Then bind it to some given port
  int retval=bind(socketfd,(struct sockaddr*)&server,sizeof server);
  if(retval==-1){
	perror("bind()");
	exit(EXIT_FAILURE);
  }
  else
	printf("Bound to %d\n",port);

  //Then we begin to listen for incoming connections
  listen(socketfd,BACKLOG);
  pthread_t pid;

  //In an infinite loop
  while(1){
  	//Allocate a memory page to be used as buffer  
	threadData *send=mmap(NULL,sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
	if(!send){
		perror("Mmap()");
		continue;
	}
	
	//Accept an incoming connection
	int clientfd;
	do
		clientfd=accept(socketfd,(struct sockaddr*)&client,&length);
	while(clientfd==-1);
	send->fd=clientfd;

	//And spawn a thread to manage that connection until it disconnects
	retval=pthread_create(&pid,NULL,threadFunc,(void*)send);
	if(retval){
		perror("pthread_create()");
		close(clientfd);
		munmap(send,sysconf(_SC_PAGE_SIZE));
		continue;
	}
  }
}

int main(){
  int port=SERVER_PORT;
  serve(port,HTTPthreadServe);
}
