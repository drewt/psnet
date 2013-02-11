CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu99 -pthread

OFILES = server.o service.o #ctable.o

all: server

server: $(OFILES) ctable.o
	$(CC) $(CFLAGS) $(OFILES) -o server

test: ctable_test.o ctable.o
	$(CC) $(CFLAGS) ctable_test.o ctable.o -o test

$(OFILES): common.h

clean:
	rm $(OFILES) ctable_test.o ctable.o server test
