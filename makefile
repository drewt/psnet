CC      = gcc
DEFINES = -DEXP_INTERVAL=1 -DINTERVAL_SECONDS=1
CFLAGS  = -Wall -Wextra -Werror -std=gnu99 -pthread $(DEFINES)

OFILES = server.o service.o #ctable.o

all: server

server: $(OFILES) ctable.o
	$(CC) $(CFLAGS) $(OFILES) -o server

test: ctable_test.o ctable.o
	$(CC) $(CFLAGS) ctable_test.o ctable.o -o test

$(OFILES): common.h
ctable.o: ctable.h

clean:
	rm $(OFILES) ctable_test.o ctable.o server test
