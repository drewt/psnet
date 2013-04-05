CC      = gcc
DEFINES = -DEXP_INTERVAL=10 -DINTERVAL_SECONDS=1 -DP2PSERV_LOG
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g -pthread $(DEFINES)

OFILES = server.o response.o client.o ctable.o
DIROFILES = dirservice.o
NODEOFILES = nodeservice.o

all: infradir infranode

infradir: $(OFILES) $(DIROFILES)
	$(CC) $(CFLAGS) $(OFILES) $(DIROFILES) -o infradir

infranode: $(OFILES) $(NODEOFILES)
	$(CC) $(CFLAGS) $(OFILES) $(NODEOFILES) -o infranode

ctable_test: ctable_test.o ctable.o
	$(CC) $(CFLAGS) ctable_test.o ctable.o -o test

$(OFILES): common.h
response.o: response.h
ctable.o: ctable.h
client.o: client.h ctable.h response.h
dirservice.o: response.h client.h

clean:
	rm -f $(OFILES) ctable_test.o infradir ctable_test
