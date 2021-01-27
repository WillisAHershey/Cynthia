//Willis A. Hershey
//Source code for Cynthia the Linux HTTP server

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "status.h"
#include "pthread_store.h"

//The following three defines represent the three arguments of the UNIX-specific socket() call
#define DOMAIN		AF_INET
#define TYPE		SOCK_STREAM
#define PROTOCOL	0

//The port at which we intend to listen for incoming traffic
#define SERVER_PORT	80

//This is the backlog-length parameter passed to listen()
#define BACKLOG		128

//This is the size of the buffer we read into for the read() calls
#define BUF_SIZE	(sysconf(_SC_PAGE_SIZE)-sizeof(threadData))

#define HTTP_VERSION	"HTTP/2.0"

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
  while(1){
	ssize_t bytes=read(d->fd,d->buf,BUF_SIZE-1);
	if(bytes==-1){
		perror("Read()");
		break;
	}
	if(!bytes)
		break;
	int request=parseRequest(d->buf);
	if(request==-1){
		sprintf(d->buf,"%s %s\r\n\r\n",STATUS_400,HTTP_VERSION);
		if(write(d->fd,d->buf,strlen(d->buf))==-1){
			perror("Write()");
			break;
		}
		else
			continue;
	}
	switch(request){
		case 0:
			printf("GET\n");
			//char *file,*protocol,*tag;
			for(int c=3;c<bytes;++c){
				if(d->buf[c]==' ')
					d->buf[c]='\0';
			}
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

//This is a generic server protocol
//First it creates a socket and binds it to some given port number
//Then it listens for incoming connections and for each connection spawns a pthread to handle the connection with some given pthread server protocol

int serve(int port,void* (threadFunc)(void*)){
  //First we create a socket for inet stream communication
  int socketfd=socket(DOMAIN,TYPE,PROTOCOL); //Resource 1
  if(socketfd==-1){
	perror("socket()");
	return EXIT_FAILURE;
  }
  printf("Socket created with file descriptor %d\n",socketfd);

  struct sockaddr_in server,client;
  socklen_t length=(socklen_t)sizeof server;
  server.sin_family=DOMAIN;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(port);

  //Then bind it to some given port
  int retval=bind(socketfd,(struct sockaddr*)&server,sizeof server);
  if(retval==-1){
	perror("bind()");
	close(socketfd);
	return EXIT_FAILURE;
  }
  printf("Bound to %d\n",port);

  //Then we begin to listen for incoming connections
  retval=listen(socketfd,BACKLOG);
  if(retval){
	perror("listen()");
	close(socketfd);
	return EXIT_FAILURE;
  }

  //Pthreads are spawned in a detached state so the parent thread does not have to join them; they enter and exit existence on their own
  pthread_attr_t attr; //resource 2
  retval=pthread_attr_init(&attr);
  if(retval){
	perror("pthread_attr_init()");
	return EXIT_FAILURE;
  }
  retval=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  if(retval){
	perror("pthread_attr_setdetachstate()");
	pthread_attr_destroy(&attr);
	close(socketfd);
	return EXIT_FAILURE;
  }

  volatile int cont=1;
  while(cont){
  	//Allocate a memory page to be used as buffer
	threadData *send=mmap(NULL,sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0); //pthread resource 1
	if(!send){
		perror("mmap()");
		continue;
	}
	
	//Accept an incoming connection
	int clientfd=accept(socketfd,(struct sockaddr*)&client,&length); //pthread resource 2
	while(clientfd==-1){
		perror("accept()");
		clientfd=accept(socketfd,(struct sockaddr*)&client,&length);
	}
	send->fd=clientfd;
	//And spawn a thread to manage that connection until it disconnects
	pthread_t pid;
	retval=pthread_create(&pid,&attr,threadFunc,(void*)send);
	while(retval){
		perror("pthread_create()");
		retval=pthread_create(&pid,&attr,threadFunc,(void*)send);
	}
  }
  pthread_attr_destroy(&attr);
  close(socketfd);
  return EXIT_SUCCESS;
}

int main(int args,char *argv[]){
  int lfd=open("log.txt",O_CREAT|O_RDWR);
  dup2(lfd,fileno(stdout));
  int retval=serve(80,HTTPthreadServe);
  exit(retval);

  /*
  char *commands[]={"exit","kill","https","http","email","help"};
  printf("Cynthia the Linux server\n\n:");
  char buffer[1024];
  buffer[1023]='\0';
  int port=-1;
  struct pidStack{
	struct pidStack *next;
	pid_t pid;
	int port;
  };
  struct pidStack *head=NULL;
  while(fgets(buffer,1023,stdin)){
	int choice=-1;
	for(int c=0;c<sizeof commands/sizeof(char*);++c)
		if(strncmp(commands[c],buffer,strlen(commands[c]))==0){
			choice=c;
			break;
		}
	switch(choice){
		case 0:
			printf("Exiting shell\n");
			exit(EXIT_SUCCESS);
			break;
		case 1:
			printf("Running servers:\n");
			for(struct pidStack *r=head;r;r=r->next){
				printf("pid:%u on port:%d\n",(unsigned int)r->pid,r->port);
				kill(r->pid,SIGINT);
			}

			break;
		case 2:
			printf("Https not yet supported. See \"help\"\n");
			break;
		case 3:
			port=80;
			pid_t h=fork();
			if(!h){

				int ret=serve(port,HTTPthreadServe);
				exit(ret);
			}
			else{
				if(h==-1){
					perror("Fork()");
				}
				else{
					struct pidStack *s=malloc(sizeof(struct pidStack));
					*s=(struct pidStack){.next=head,.pid=h,.port=port};
					head=s;
				}
			}
			break;
		case 4:
			printf("Email not yet supported. See \"help\"\n");
			break;
		case 5:
			printf("Help:\n\nexit: Exits from shell prompt leaving all spawned servers running\nkill: Produces a list of running servers and gives you the option to terminate them\nhttps: NOT YET IMPLEMENTED\nhttp: Spawns an http server\nemail: NOT YET IMPLEMENTED\nhelp: Displays this message\n\n");
			break;
	}
	printf(":");
  }*/
}
