#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>  // for size_t

typedef struct memory_block {
    size_t block_size;
    int    is_free;
    struct memory_block *phys_next;
    struct memory_block *phys_prev;
    struct memory_block *free_next;
    struct memory_block *free_prev;
} mem_block_t;

typedef mem_block_t *(*finder_func_t)(size_t size);

void *ff_malloc(size_t size);
void ff_free(void *ptr);

void *bf_malloc(size_t size);
void bf_free(void *ptr);

unsigned long get_data_segment_size();
long long get_data_segment_free_space_size();

static void *allocate_memory(size_t size, finder_func_t strategy);
static void release_memory(void *ptr);
static mem_block_t *locate_first_fit(size_t size);
static mem_block_t *locate_best_fit(size_t size);
static mem_block_t *grow_heap(size_t size);
static void attach_to_free_list(mem_block_t *block);
static void detach_from_free_list(mem_block_t *block);
static void merge_with_neighbors(mem_block_t *block);
static void divide_block(mem_block_t *block, size_t requested);

#endif
