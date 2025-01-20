TARGETS=mmap_basic mmap_basic2 pipe_basic

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

mmap_basic: mmap_basic.c
	gcc -O3 -o $@ $< -lpthread

mmap_basic2: mmap_basic2.c
	gcc -O3 -o $@ $< -lpthread

pipe_basic: pipe_basic.c
	gcc -O3 -o $@ $<

