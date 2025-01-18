#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    int free;
    int magic; // to check if the block is valid
} block_meta_t;

void* ff_malloc(size_t size);
void ff_free(void* ptr);

void* bf_malloc(size_t size);
void bf_free(void* ptr);

unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();

#endif
