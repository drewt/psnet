
BIN = bin

all:
	(cd src; $(MAKE))

clean:
	rm -f $(BIN)/*

dirmem: infradir
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infradir 10000 6666

nodemem: infranode
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/infranode 10000 6666

