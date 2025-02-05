// my_malloc.h
#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>
#include <stdio.h>

typedef struct metadata {
    size_t size;
    int isfree;
    struct metadata *next;
    struct metadata *prev;
} Metadata;

void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();

#endif // MY_MALLOC_H
