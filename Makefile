#Willis A. Hershey
#Makefile for Cynthia the http server

cynthia: cynthia.c pthread_store.o
	gcc -o cynthia -Wall -O3 -lpthread cynthia.c pthread_store.o
	
pthread_store.o: pthread_store.c pthread_store.h
	gcc -c -Wall -O3 -lpthread pthread_store.c

.PHONY: clean

clean:
	rm cynthia pthread_store.o
