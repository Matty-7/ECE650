#include "my_malloc.h"
#include <unistd.h>    // for sbrk
#include <stdint.h>    
#include <stddef.h>    // for NULL, size_t

static block_meta_t *global_base = NULL;  // Head pointer of the linked list
static block_meta_t *global_last = NULL;  // Tail pointer (for appending blocks after sbrk)

// total heap size and free space size (including metadata)
static unsigned long total_size = 0;
static unsigned long total_free_size = 0;


static block_meta_t *get_block_ptr(void *ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}


static block_meta_t* request_space(block_meta_t* last, size_t size);
static void split_block(block_meta_t* block, size_t size);
static void coalesce_block(block_meta_t* block);

// first fit
static block_meta_t* find_free_block_ff(size_t size);
// best fit
static block_meta_t* find_free_block_bf(size_t size);


unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

static block_meta_t *last_alloc = NULL; 

static block_meta_t* find_free_block_ff(size_t size) {
    // search from last_alloc
    block_meta_t* current = last_alloc ? last_alloc : global_base;
    // if the list is single-linked, we can record the starting point first
    block_meta_t* start = current;

    // search from last_alloc
    while (current) {
        if (current->free && current->size >= size) {
            last_alloc = current;  // update last_alloc
            return current;
        }
        current = current->next;
    }
    // if not found, search from the head
    current = global_base;
    while (current && current != start) {
        if (current->free && current->size >= size) {
            last_alloc = current;  
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// search for the best fit block in the list
static block_meta_t* find_free_block_bf(size_t size) {
    block_meta_t *current = global_base;
    block_meta_t *best_fit = NULL;
    size_t smallest_diff = (size_t)-1;

    while (current) {
        if (current->free && current->size >= size) {
            size_t diff = current->size - size;
            if (diff < smallest_diff) {
                smallest_diff = diff;
                best_fit = current;
                if (diff == 0) {
                    
                    return best_fit;
                }
            }
        }
        current = current->next;
    }
    return best_fit;
}

// if no free block is available (or not enough space), request space from system
static block_meta_t* request_space(block_meta_t* last, size_t size) {
    // sbrk(0) returns current break address as starting address for new block
    block_meta_t* block = sbrk(0);
    void *request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*)-1) {
        // sbrk failed
        return NULL;
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;
    block->prev = last;

    // if the previous last is not NULL, the list has nodes, update last->next
    if (last) {
        last->next = block;
    }

    if (!global_base) {
        global_base = block;
    }
    global_last = block;

    // update heap size
    total_size += (size + sizeof(block_meta_t));

    return block;
}

// split a large block if it has too much space
static void split_block(block_meta_t* block, size_t size) {
    // check if there is enough space for new block_meta_t + at least 1 byte
    if (block->size >= size + sizeof(block_meta_t) + 1) {
        // new block address: offset from current block by size + metadata
        char* block_address = (char*)block;
        block_meta_t* new_block = (block_meta_t*)(block_address
                                 + sizeof(block_meta_t)
                                 + size);

        new_block->size = block->size - size - sizeof(block_meta_t);
        new_block->free = 1;
        new_block->next = block->next;
        new_block->prev = block;

        if (block->next) {
            block->next->prev = new_block;
        }
        block->next = new_block;

        // shrink original block
        block->size = size;

        // update total_free_size: new block is free
        total_free_size += (new_block->size + sizeof(block_meta_t));
    }
}

// in-place coalescing: only merge with adjacent blocks, no full list scan
static void coalesce_block(block_meta_t* block) {
    while (block->next && block->next->free) {
        block_meta_t* next_block = block->next;
        // merge next_block into current block
        block->size += (next_block->size + sizeof(block_meta_t));

        // adjust free space count
        // total_free_size -= sizeof(block_meta_t);

        // remove next_block from list
        block->next = next_block->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            global_last = block;
        }
    }

    // merge with previous free block
    while (block->prev && block->prev->free) {
        block_meta_t* prev_block = block->prev;
        prev_block->size += (block->size + sizeof(block_meta_t));

        // update free space count and remove block from list
        // total_free_size -= sizeof(block_meta_t);

        // remove block from list
        prev_block->next = block->next;
        if (block->next) {
            block->next->prev = prev_block;
        } else {
            global_last = prev_block;
        }
        block = prev_block; 
    }
}

void* ff_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // search for free block in the list
    block_meta_t* block = find_free_block_ff(size);
    if (!block) {
        // no free block found, request space from system
        block = request_space(global_last, size);
        if (!block) {
            // sbrk failed
            return NULL;
        }
    } else {
        // found a free block
        block->free = 0;
        // split
        // note: first subtract the whole block from total_free_size
        // then add back if split out a new block
        total_free_size -= (block->size + sizeof(block_meta_t));
        split_block(block, size);
    }

    // return pointer to data area (skip metadata)
    return (char*)block + sizeof(block_meta_t);
}

void ff_free(void *ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block_ptr = get_block_ptr(ptr);
    block_ptr->free = 1;

    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    coalesce_block(block_ptr);
}

void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    block_meta_t* block = find_free_block_bf(size);
    if (!block) {
        // no free block found, request space from system
        block = request_space(global_last, size);
        if (!block) {
            return NULL;
        }
    } else {
        // found a free block
        block->free = 0;
        total_free_size -= (block->size + sizeof(block_meta_t));
        split_block(block, size);
    }

    return (char*)block + sizeof(block_meta_t);
}

void bf_free(void* ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block_ptr = get_block_ptr(ptr);
    block_ptr->free = 1;
    total_free_size += (block_ptr->size + sizeof(block_meta_t));
    coalesce_block(block_ptr);
}
