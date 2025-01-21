#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>  // for size_t

void* ff_malloc(size_t size);
void ff_free(void* ptr);

void* bf_malloc(size_t size);
void bf_free(void* ptr);

unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();

#endif // MY_MALLOC_H
