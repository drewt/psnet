CC      = gcc
DEFINES = -DEXP_INTERVAL=10 -DINTERVAL_SECONDS=1 -DP2PSERV_LOG
CFLAGS  = -Wall -Wextra -Werror -std=gnu99 -g -pthread $(DEFINES)

OFILES = server.o service.o client.o ctable.o

all: server

server: $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o server -lm

ctable_test: ctable_test.o ctable.o
	$(CC) $(CFLAGS) ctable_test.o ctable.o -o test

$(OFILES): common.h
ctable.o: ctable.h
client.o: client.h

clean:
	rm -f $(OFILES) ctable_test.o server ctable_test
