CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread

all: lab2lvcN32451_server lab2lvcN32451_client

lab2lvcN32451_server: lab2lvcN32451_server.c
	$(CC) $(CFLAGS) -g -o $@ $<

lab2lvcN32451_client: lab2lvcN32451_client.c
	$(CC) $(CFLAGS) -g -o $@ $<

clean:
	rm -f lab2lvcN32451_server lab2lvcN32451_client
