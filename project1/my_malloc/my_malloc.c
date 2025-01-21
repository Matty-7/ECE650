#include "my_malloc.h"
#include <assert.h>
#include <unistd.h>    // for sbrk
#include <string.h>    // for NULL
#include <stdio.h>

#define ALLOC_UNIT (4096)

typedef struct block_t {
    size_t size;           
    int is_free;
    struct block_t* next_physical;
    struct block_t* prev_physical;

    struct block_t* next_free;
    struct block_t* prev_free;
} block_t;

static block_t* free_list_head = NULL;

static block_t* heap_tail = NULL;

static unsigned long data_segment_size = 0;
static unsigned long data_segment_free_space_size = 0; 

static void* allocate_memory(size_t size, block_t* (*find_strategy)(size_t));
static block_t* extend_heap(size_t size);
static block_t* split_block(block_t* block, size_t size);
static void merge_free_blocks(block_t* block);
static void free_list_insert(block_t* block);
static void free_list_remove(block_t* block);
static block_t* find_first_fit(size_t size);
static block_t* find_best_fit(size_t size);
static size_t align8(size_t size);


// FF
void *ff_malloc(size_t size) {
    return allocate_memory(size, find_first_fit);
}
void ff_free(void *ptr) {
    if (!ptr) return;
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    if (block->is_free) {
        
        return;
    }
    block->is_free = 1;
    data_segment_free_space_size += (block->size + sizeof(block_t));
    // according to the address order
    free_list_insert(block);
    merge_free_blocks(block);
}

// BF
void *bf_malloc(size_t size) {
    return allocate_memory(size, find_best_fit);
}
void bf_free(void *ptr) {
    ff_free(ptr); 
}

unsigned long get_data_segment_size() {
    return data_segment_size;
}
unsigned long get_data_segment_free_space_size() { 
    return data_segment_free_space_size;
}

static void* allocate_memory(size_t size, block_t* (*find_strategy)(size_t)) {
    if (size == 0) return NULL;
    size = align8(size);

    if (free_list_head) {
        block_t* block = find_strategy(size);
        if (block) {
            free_list_remove(block); // block->is_free=0
            if (block->size >= size + sizeof(block_t) + 8) {
                block_t* new_free = split_block(block, size);
                free_list_insert(new_free);
            }
            return (char*)block + sizeof(block_t);
        }
    }

    block_t* new_block = extend_heap(size);
    if (!new_block) return NULL; 
    return (char*)new_block + sizeof(block_t);
}

static block_t* extend_heap(size_t size) {
    size_t ask = size + sizeof(block_t);
    if (ask < ALLOC_UNIT) ask = ALLOC_UNIT;

    void* ptr = sbrk(ask);
    if (ptr == (void*)-1) return NULL;

    data_segment_size += ask;

    block_t* block = (block_t*)ptr;
    block->size   = size;
    block->is_free = 0;
    block->prev_free = heap_tail;
    block->next_free = NULL;
    block->prev_physical = heap_tail;
    block->next_physical = NULL;
    if (heap_tail) {
        heap_tail->next_physical = block;
    }
    heap_tail = block;

    size_t leftover = ask - sizeof(block_t) - size;
    if (leftover >= sizeof(block_t) + 8) {
        block_t* new_free = (block_t*)((char*)block + sizeof(block_t) + size);
        new_free->size = leftover - sizeof(block_t);
        new_free->is_free = 1;
        new_free->prev_free = new_free->next_free = NULL;
        new_free->prev_physical = block;
        new_free->next_physical = NULL;

        heap_tail = new_free;   
        block->next_physical = new_free;

        data_segment_free_space_size += (new_free->size + sizeof(block_t));

        free_list_insert(new_free);
    }
    return block;
}

// split block: the block (allocated) is reduced to size, and the new free block is inserted at the end of the block
static block_t* split_block(block_t* block, size_t size) {
    char* split_addr = (char*)block + sizeof(block_t) + size;
    block_t* new_free = (block_t*)split_addr;

    new_free->size   = block->size - size - sizeof(block_t);
    new_free->is_free = 1;
    new_free->prev_free = new_free->next_free = NULL;

    new_free->prev_physical = block;
    new_free->next_physical = block->next_physical;
    if (new_free->next_physical) {
        new_free->next_physical->prev_physical = new_free;
    } else {
        if (block == heap_tail) {
            heap_tail = new_free;
        }
    }
    block->next_physical = new_free;

    block->size = size;

    return new_free;
}

// merge block with its adjacent free block
static void merge_free_blocks(block_t* block) {
    block_t* right = block->next_physical;
    if (right && right->is_free) {
        
        free_list_remove(right);
        
        block->size += (sizeof(block_t) + right->size);
        block->next_physical = right->next_physical;
        if (right->next_physical) {
            right->next_physical->prev_physical = block;
        } else {
            heap_tail = block; // right was the tail
        }
    }

    // 2) check left neighbor
    block_t* left = block->prev_physical;
    if (left && left->is_free) {
        free_list_remove(block);
        left->size += (sizeof(block_t) + block->size);
        left->next_physical = block->next_physical;
        if (block->next_physical) {
            block->next_physical->prev_physical = left;
        } else {
            heap_tail = left;
        }
        block = left;
    }
}

// insert block into free list according to the address order
static void free_list_insert(block_t* block) {
    if (!free_list_head) {
        block->next_free = block->prev_free = NULL;
        free_list_head = block;
        return;
    }
    if (block < free_list_head) {
        block->next_free = free_list_head;
        block->prev_free = NULL;
        free_list_head->prev_free = block;
        free_list_head = block;
        return;
    }
    block_t* cur = free_list_head;
    while (cur->next_free && cur->next_free < block) {
        cur = cur->next_free;
    }
    block->next_free = cur->next_free;
    block->prev_free = cur;
    if (cur->next_free) {
        cur->next_free->prev_free = block;
    }
    cur->next_free = block;
}

// remove block from free list
static void free_list_remove(block_t* block) {
    if (!block->is_free) {
        return;
    }
    data_segment_free_space_size -= (block->size + sizeof(block_t));
    block->is_free = 0;

    // fix list pointers
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        free_list_head = block->next_free;
    }
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }
    block->next_free = block->prev_free = NULL;
}

// first-fit: find the first free block that is large enough
static block_t* find_first_fit(size_t size) {
    block_t* cur = free_list_head;
    while (cur) {
        if (cur->size >= size) {
            return cur;
        }
        cur = cur->next_free;
    }
    return NULL;
}

// best-fit: find the smallest free block that is large enough
static block_t* find_best_fit(size_t size) {
    block_t* best = NULL;
    block_t* cur = free_list_head;
    while (cur) {
        if (cur->size >= size) {
            if (!best || cur->size < best->size) {
                best = cur;
                if (cur->size == size) {
                    return cur;
                }
            }
        }
        cur = cur->next_free;
    }
    return best;
}

// align to 8 bytes
static size_t align8(size_t size) {
    return (size + 7) & ~(size_t)7;
}

