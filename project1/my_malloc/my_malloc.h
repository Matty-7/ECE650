#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h> // size_t

typedef struct block_meta {
    size_t size; // does not include the size of the meta data
    struct block_meta *next;
    struct block_meta *prev;
    int free;
} block_meta_t;

void* ff_malloc(size_t size);
void ff_free(void* ptr);

void* bf_malloc(size_t size);
void bf_free(void* ptr);

unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();

#endif
