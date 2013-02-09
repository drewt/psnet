CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pthread

OFILES = server.o service.o

all: server

server: $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o server

$(OFILES): common.h

clean:
	rm $(OFILES) server
