SHELL = /bin/sh

.SUFFIXES:
.PHONY: clean depclean dirclean nodeclean dirmem nodemem

SRC = src
BIN = bin
INC = include
DEP = dep
DOC = doc

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -g
ALLCFLAGS = -I $(INC) $(CFLAGS)
CPPFLAGS = -DPSNETLOG -D_Noreturn=__attribute\(\(noreturn\)\)
LDFLAGS = -pthread

# targets
OFILES = $(BIN)/request.o $(BIN)/response.o $(BIN)/client.o $(BIN)/deltalist.o \
	  $(BIN)/udpserver.o $(BIN)/tcpserver.o $(BIN)/ini.o $(BIN)/common.o \
	  $(BIN)/jsmn.o
DIROFILES = $(BIN)/dirservice.o
NODEOFILES = $(BIN)/nodeservice.o $(BIN)/router.o $(BIN)/dirclient.o \
	     $(BIN)/nodelist.o $(BIN)/msgcache.o
ALLOFILES = $(OFILES) $(DIROFILES) $(NODEOFILES)
DFILES = $(addprefix $(DEP)/,$(addsuffix .d,$(basename $(notdir $(ALLOFILES)))))
XFILES = $(BIN)/psdird $(BIN)/psnoded

all: $(XFILES)

-include $(DFILES)

$(BIN)/psdird: $(OFILES) $(DIROFILES)
	$(CC) $(ALLCFLAGS) $(LDFLAGS) $^ -o $@

$(BIN)/psnoded: $(OFILES) $(NODEOFILES)
	$(CC) $(ALLCFLAGS) $(LDFLAGS) $^ -o $@

# build binaries in $(BIN)
$(BIN)/%.o: $(SRC)/%.c
	$(CC) -c $(ALLCFLAGS) $(CPPFLAGS) $< -o $@

# automatically generate dependencies
$(DEP)/%.d: $(SRC)/%.c
	@echo "Generating dependencies for $<"
	@echo "$@ $(BIN)/`$(CC) -MM -I $(INC) $(CPPFLAGS) $<`" > $@

clean:
	rm -f $(BIN)/*

depclean:
	rm -f $(DEP)/*

dirclean:
	rm -f $(OFILES) $(DIROFILES) $(BIN)/psdird

nodeclean:
	rm -f $(OFILES) $(NODEOFILES) $(BIN)/psnoded

dirmem: $(BIN)/psdird
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psdird

nodemem: $(BIN)/psnoded
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psnoded

