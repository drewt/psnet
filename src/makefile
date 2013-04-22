CC      = gcc
DEFINES = -DPSNETLOG -D_Noreturn=__attribute\(\(noreturn\)\)
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g -pthread \
	  -I $(INC) $(DEFINES)

OFILES = $(BIN)/request.o $(BIN)/response.o $(BIN)/client.o $(BIN)/deltalist.o \
	  $(BIN)/udpserver.o $(BIN)/tcpserver.o $(BIN)/ini.o $(BIN)/common.o \
	  $(BIN)/jsmn.o
DIROFILES = $(BIN)/dirservice.o
NODEOFILES = $(BIN)/nodeservice.o $(BIN)/router.o $(BIN)/dirclient.o \
	     $(BIN)/nodelist.o $(BIN)/msgcache.o

XFILES = $(BIN)/psdird $(BIN)/psnoded

INC = ../include
BIN = ../bin

$(BIN)/%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

all: $(XFILES)

$(BIN)/psdird: $(OFILES) $(DIROFILES)
	$(CC) $(CFLAGS) $(OFILES) $(DIROFILES) -o $(BIN)/psdird

$(BIN)/psnoded: $(OFILES) $(NODEOFILES)
	$(CC) $(CFLAGS) $(OFILES) $(NODEOFILES) -o $(BIN)/psnoded -lm

$(OFILES): $(INC)/common.h

# common
$(BIN)/common.o: $(INC)/jsmn.h
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

dirmem: $(BIN)/psdird
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psdird 10000 6666

nodemem: $(BIN)/psnoded
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psnoded 10000 5555

