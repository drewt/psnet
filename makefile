CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-int-to-pointer-cast \
	 -Wno-pointer-to-int-cast -pthread

OFILES = server.o service.o

all: server

server: $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o server

server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c -o server.o

service.o: service.c common.h
	$(CC) $(CFLAGS) -c service.c -o service.o

clean:
	rm $(OFILES) mtserver

