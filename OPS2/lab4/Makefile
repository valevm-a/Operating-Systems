CC=gcc
CFLAGS= -std=gnu99 -Wall

all: server client
server: server.c	
	$(CC) $(CFLAGS) -o server server.c -lrt
client: client.c	
	$(CC) $(CFLAGS) -o client client.c -lrt
.PHONY: clean all
clean:
	rm server client
