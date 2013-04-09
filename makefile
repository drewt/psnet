CC      = gcc
DEFINES = -DEXP_INTERVAL=10 -DINTERVAL_SECONDS=1 -DP2PSERV_LOG
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g -pthread $(DEFINES)

OFILES = request.o response.o client.o ctable.o
DIROFILES = dirservice.o tcpserver.o 
NODEOFILES = nodeservice.o udpserver.o router.o dirclient.o jsmn.o

XFILES = infradir infranode

all: $(XFILES)

infradir: $(OFILES) $(DIROFILES)
	$(CC) $(CFLAGS) $(OFILES) $(DIROFILES) -o infradir

infranode: $(OFILES) $(NODEOFILES)
	$(CC) $(CFLAGS) $(OFILES) $(NODEOFILES) -o infranode -lm

ctable_test: ctable_test.o ctable.o
	$(CC) $(CFLAGS) ctable_test.o ctable.o -o test

$(OFILES): common.h
tcpserver.o: tcp.h
udpserver.o: udp.h
request.o: tcp.h
response.o: response.h
ctable.o: ctable.h
client.o: client.h ctable.h response.h
dirservice.o: tcp.h ctable.h response.h client.h
nodeservice.o: udp.h ctable.h router.h client.h jsmn.h
dirclient.o: dirclient.h tcp.h jsmn.h
jsmn.o: jsmn.h

clean:
	rm -f $(OFILES) $(DIROFILES) $(NODEOFILES) ctable_test.o $(XFILES) ctable_test
