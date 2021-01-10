#ifndef PTHREAD_STORE
#define PTHREAD_STORE

int pthread_store(void*);
void* pthread_recall();
void* pthread_discard();
int pthread_discard_all();
int pthread_store_close();

#endif
