#include <stdlib.h>
#include <pthread.h>
#include "pthread_store.h"

#define PTHREAD_STORE_SUCCESS 0
#define PTHREAD_STORE_FAILURE -1

typedef struct storelistStruct{
  struct storelistStruct *next;
  void *store;
  pthread_t tid;
}storelist;

static pthread_mutex_t mux=PTHREAD_MUTEX_INITIALIZER;
static storelist *head=NULL;

int pthread_store(void *store){
  pthread_t self=pthread_self();
  storelist *pt=malloc(sizeof(storelist));
  if(!pt)
	return PTHREAD_STORE_FAILURE;
  pthread_mutex_lock(&mux);
  *pt=(storelist){.next=head,.store=store,.tid=self};
  head=pt;
  pthread_mutex_unlock(&mux);
  return PTHREAD_STORE_SUCCESS;
}

void* pthread_recall(){
  pthread_t self=pthread_self();
  pthread_mutex_lock(&mux);
  void *out=NULL;
  for(storelist *pt=head;pt;pt=pt->next)
	if(pthread_equal(pt->tid,self)){
		out=pt->store;
		break;
	}
  pthread_mutex_unlock(&mux);
  return out;
}

void* pthread_discard(){
  pthread_t self=pthread_self();
  void *out=NULL;
  pthread_mutex_lock(&mux);
  if(!head)
	goto finished;
  if(pthread_equal(head->tid,self)){
	storelist *pt=head;
	head=pt->next;
	out=pt->store;
	free(pt);
	goto finished;
  }
  for(storelist *b=head,*r=head->next;r;b=r,r=r->next)
	if(pthread_equal(r->tid,self)){
		b->next=r->next;
		out=r->store;
		free(r);
		break;
	}
finished:
  pthread_mutex_unlock(&mux);
  return out;
}

int pthread_discard_all(){
  pthread_t self=pthread_self();
  int out=0;
  pthread_mutex_lock(&mux);
  while(head&&pthread_equal(head->tid,self)){
	++out;
	storelist *pt=head;
	head=pt->next;
	free(pt);
  }
  if(!head)
	goto finished;
  for(storelist *b=head,*r=head->next;r;)
	if(pthread_equal(r->tid,self)){
		++out;
		b->next=r->next;
		free(r);
		r=b->next;
	}
  	else{
		b=r;
		r=r->next;
	}
finished:
  pthread_mutex_unlock(&mux);
  return out;
}

int pthread_store_close(){
  while(head){
	storelist *pt=head;
	head=pt->next;
	free(pt);
  }
  return pthread_mutex_destroy(&mux);
}
