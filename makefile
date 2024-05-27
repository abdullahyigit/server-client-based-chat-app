CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lssl -lcrypto -pthread

all: server client

server: server.o
	$(CC) $(CFLAGS) server.o -o server $(LDFLAGS)

client: client.o
	$(CC) $(CFLAGS) client.o -o client $(LDFLAGS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o server client

.PHONY: all clean

