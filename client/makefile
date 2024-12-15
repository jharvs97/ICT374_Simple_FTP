# Makefile for client

myftp: client.o token.o stream.o
	gcc client.o token.o stream.o -o myftp

client.o: client.c token.h stream.h protocol.h
	gcc -c -Wall client.c

token.o: token.c token.h
	gcc -c -Wall token.c

stream.o: stream.c stream.h
	gcc -c -Wall stream.c

clean:
	rm *.o