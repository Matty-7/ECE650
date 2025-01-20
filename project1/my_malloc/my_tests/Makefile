MY_MALLOC_SRC = ../my_malloc.c
MY_MALLOC_HDR = ../my_malloc.h

TEST_SRCS = test1.c test2.c test3.c test4.c test5.c test6.c test7.c test8.c test9.c test11.c test12.c

TEST_BINS = $(TEST_SRCS:.c=)


CFLAGS = -g -O2 -I.. 
LDLIBS = -lpthread  

.PHONY: all clean

all: $(TEST_BINS)

%: %.c $(MY_MALLOC_SRC) $(MY_MALLOC_HDR)
	$(CC) $(CFLAGS) $< $(MY_MALLOC_SRC) -o $@ $(LDLIBS)

clean:
	rm -f $(TEST_BINS) *.o
