CC = g++
CFLAGS = -O3
LIBS = -lpqxx -lpq

all: test

test: main.cpp exerciser.cpp query_funcs.cpp
	$(CC) $(CFLAGS) -o test main.cpp exerciser.cpp query_funcs.cpp $(LIBS)

clean:
	rm -f *~ *.o test

clobber:
	rm -f *~ *.o test
