CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu99 -pthread

OFILES = server.o service.o #ctable.o

all: server

server: $(OFILES) ctable.o
	$(CC) $(CFLAGS) $(OFILES) -o server

$(OFILES): common.h

clean:
	rm $(OFILES) server
