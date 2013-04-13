CC      = gcc
DEFINES = -DPSNETLOG -D_Noreturn=__attribute\(\(noreturn\)\)
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g -pthread \
	  -I $(INC) $(DEFINES)

OFILES = $(BIN)/request.o $(BIN)/response.o $(BIN)/client.o $(BIN)/deltalist.o \
	  $(BIN)/udpserver.o $(BIN)/tcpserver.o $(BIN)/ini.o
DIROFILES = $(BIN)/dirservice.o
NODEOFILES = $(BIN)/nodeservice.o $(BIN)/router.o $(BIN)/dirclient.o \
	     $(BIN)/nodelist.o $(BIN)/msgcache.o $(BIN)/jsmn.o

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

$(OFILES): $(INC)/common.h

# common
$(BIN)/tcpserver.o: $(INC)/tcp.h
$(BIN)/udpserver.o: $(INC)/udp.h
$(BIN)/request.o: $(INC)/tcp.h
$(BIN)/response.o: $(INC)/response.h
$(BIN)/deltalist.o: $(INC)/deltalist.h
$(BIN)/client.o: $(INC)/client.h $(INC)/response.h $(INC)/deltalist.h \
    $(INC)/udp.h
$(BIN)/jsmn.o: $(INC)/jsmn.h
$(BIN)/inih.o: $(INC)/ini.h

# directory
$(BIN)/dirservice.o: $(INC)/tcp.h $(INC)/response.h $(INC)/client.h

# router
$(BIN)/nodeservice.o: $(INC)/udp.h $(INC)/client.h $(INC)/router.h \
    $(INC)/jsmn.h
$(BIN)/dirclient.o: $(INC)/dirclient.h $(INC)/nodelist.h $(INC)/tcp.h \
    $(INC)/jsmn.h
$(BIN)/nodelist.o: $(INC)/nodelist.h $(INC)/jsmn.h
$(BIN)/router.o: $(INC)/router.h $(INC)/udp.h $(INC)/nodelist.h \
    $(INC)/dirclient.h

clean:
	rm -f $(BIN)/*

dirmem: $(BIN)/infradir
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infradir 10000 6666

nodemem: $(BIN)/infranode
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infranode 10000 5555

