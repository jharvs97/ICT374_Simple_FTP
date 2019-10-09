# Makefile for server

myftpd: server.o daemon.o stream.o
	gcc server.o daemon.o stream.o -o myftpd

server.o: server.c daemon.h	stream.h protocol.h
	gcc -c server.c

daemon.o: daemon.c daemon.h
	gcc -c daemon.c

stream.o: stream.c stream.h
	gcc -c stream.c

clean:
	rm *.o