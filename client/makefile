# Makefile for client

myftp: client.o token.o stream.o
	gcc client.o token.o stream.o -o myftp

client.o: client.c token.h stream.h protocol.h
	gcc -c client.c

token.o: token.c token.h
	gcc -c token.c

stream.o: stream.c stream.h
	gcc -c stream.c

clean:
	rm *.o