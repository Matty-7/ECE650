#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>

// Function declarations for First Fit
void* ff_malloc(size_t size);
void ff_free(void* ptr);

// Function declarations for Best Fit
void* bf_malloc(size_t size);
void bf_free(void* ptr);

// Performance metrics functions
unsigned long get_data_segment_size();
long long get_data_segment_free_space_size();

// Block metadata structure
typedef struct Block {
    size_t size;
    int is_free;
    struct Block* next_phys;
    struct Block* prev_phys;
    struct Block* next_free;
    struct Block* prev_free;
} Block;

// Function pointer type for finding blocks
typedef Block* (*find_block_func_t)(size_t);

// Internal helper functions
void* internal_malloc(size_t size, find_block_func_t find_block);
void internal_free(void* ptr);
Block* find_first_fit(size_t size);
Block* find_best_fit(size_t size);
Block* request_memory_from_heap(size_t size);
void add_to_free_list(Block* block);
void remove_from_free_list(Block* block);
void merge_blocks(Block* block);
Block* split_block(Block* block, size_t size);

#ifndef ALLOC_UNIT
#define ALLOC_UNIT (3 * sysconf(_SC_PAGESIZE))
#endif

#endif // MY_MALLOC_H
