CC      = gcc
DEFINES = -DEXP_INTERVAL=10 -DINTERVAL_SECONDS=1 -DP2PSERV_LOG
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g -pthread \
	  -I $(INC) $(DEFINES)

OFILES = $(BIN)/request.o $(BIN)/response.o $(BIN)/client.o $(BIN)/ctable.o
DIROFILES = $(BIN)/dirservice.o $(BIN)/tcpserver.o 
NODEOFILES = $(BIN)/nodeservice.o $(BIN)/udpserver.o $(BIN)/router.o \
	     $(BIN)/dirclient.o $(BIN)/nodelist.o $(BIN)/jsmn.o

XFILES = $(BIN)/infradir $(BIN)/infranode

INC = ../include
BIN = ../bin

$(BIN)/%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

all: $(XFILES)

$(BIN)/infradir: $(OFILES) $(DIROFILES)
	$(CC) $(CFLAGS) $(OFILES) $(DIROFILES) -o $(BIN)/infradir

$(BIN)/infranode: $(OFILES) $(NODEOFILES)
	$(CC) $(CFLAGS) $(OFILES) $(NODEOFILES) -o $(BIN)/infranode -lm

$(BIN)/ctable_test: $(BIN)/ctable_test.o $(BIN)/ctable.o
	$(CC) $(CFLAGS) $(BIN)/ctable_test.o $(BIN)/ctable.o -o $(BIN)/test

$(OFILES): $(INC)/common.h
$(BIN)/tcpserver.o: $(INC)/tcp.h
$(BIN)/udpserver.o: $(INC)/udp.h
$(BIN)/request.o: $(INC)/tcp.h
$(BIN)/response.o: $(INC)/response.h
$(BIN)/ctable.o: $(INC)/ctable.h
$(BIN)/client.o: $(INC)/client.h $(INC)/ctable.h $(INC)/response.h
$(BIN)/dirservice.o: $(INC)/tcp.h $(INC)/ctable.h $(INC)/response.h $(INC)/client.h
$(BIN)/nodeservice.o: $(INC)/udp.h $(INC)/ctable.h $(INC)/router.h $(INC)/client.h \
    $(INC)/jsmn.h
$(BIN)/dirclient.o: $(INC)/dirclient.h $(INC)/tcp.h $(INC)/jsmn.h
$(BIN)/nodelist.o: $(INC)/nodelist.h $(INC)/jsmn.h
$(BIN)/jsmn.o: $(INC)/jsmn.h

clean:
	rm -f $(OFILES) $(DIROFILES) $(NODEOFILES) $(BIN)/ctable_test.o $(XFILES) \
	    $(BIN)/ctable_test

dirmem: infradir
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infradir 10000 6666

nodemem: infranode
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infranode 10000 6666

