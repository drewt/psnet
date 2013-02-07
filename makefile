CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-int-to-pointer-cast \
	 -Wno-pointer-to-int-cast -pthread

OFILES = server.o service.o

all: server

server: $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o server

clean:
	rm $(OFILES) server
