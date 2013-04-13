
BIN = bin

all:
	(cd src; $(MAKE))

clean:
	rm -f $(BIN)/*

dirmem: all
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psdird 10000 6666

nodemem: all
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
	    --num-callers=20 --track-fds=yes $(BIN)/psnoded 5555

