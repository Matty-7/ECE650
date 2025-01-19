#include "my_malloc.h"
#include <unistd.h> // sbrk
#include <stddef.h>   // size_t
#include <stdint.h>

static block_meta_t *global_base = NULL; // the head of the linked list of memory blocks

static unsigned long total_size = 0; // includes the size of the meta data

static unsigned long total_free_size = 0; // includes the size of the meta data

// get access to the meta data of a memory block

static block_meta_t *get_block_ptr(void *ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}

// find the appropriate free block for the allocation (first fit)
static block_meta_t *find_free_block_ff(block_meta_t **last, size_t size) {
    block_meta_t *current = global_base;

    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        *last = current;
        current = current->next;
    }
    return NULL;
}

// best fit search
block_meta_t *find_free_block_bf(block_meta_t **last, size_t size) {
    block_meta_t *current = global_base;
    block_meta_t *best_fit = NULL;
    size_t smallest_diff = SIZE_MAX;

    while (current) {
        if (current->free && current->size >= size) {
            size_t diff = current->size - size;
            if (diff < smallest_diff) {
                smallest_diff = diff;
                best_fit = current;
                if (diff == 0) {
                    return best_fit;
                };
            }
        }
        *last = current;
        current = current->next;
    }
    return best_fit;
}

// request more memory from the system for the heap
block_meta_t *request_space(block_meta_t * last, size_t size) {
    
    block_meta_t *block = sbrk(0);
    void *request = sbrk(size + sizeof(block_meta_t));

    if (request == (void*) -1) {
        return NULL;
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;
    
    if (last) {
        last->next = block;
    }

    total_size += size + sizeof(block_meta_t);
    return block;
}

// split the block if it is larger than the requested size
void split_block(block_meta_t *block, size_t size) {
    if (block->size >= size + sizeof(block_meta_t) + 1) {
        block_meta_t *new_block = (block_meta_t*)((char*)block + sizeof(block_meta_t) + size);
        new_block->size = block->size - size - sizeof(block_meta_t);
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;

        total_free_size += new_block->size + sizeof(block_meta_t);
    }
}

// coalesce free blocks that are next to each other
void coalesce() {
    block_meta_t *current = global_base;
    while (current && current->next) {
        
        if (current->free && current->next->free) {
            
            current->size += current->next->size + sizeof(block_meta_t);

            total_free_size -= sizeof(block_meta_t);

            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* ff_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    block_meta_t *block;
    block_meta_t *last = global_base;

    if (!global_base) {
        block = request_space(NULL, size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block = find_free_block_ff(&last, size);
        if (!block) {
            
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            
            block->free = 0;
            
            if (block->size > size) {
                
                split_block(block, size);
            }
            
            total_free_size -= (block->size + sizeof(block_meta_t));
        }
    }
    
    return (block + 1);
}

void ff_free(void *ptr) {
    if (!ptr) return;

    block_meta_t *block_ptr = get_block_ptr(ptr);

    block_ptr->free = 1;

    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    coalesce();
}


void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    block_meta_t *block;
    block_meta_t *last = global_base;

    if (!global_base) {
        block = request_space(NULL, size);
        if (!block) return NULL;
        global_base = block;
    } else {
        
        block = find_free_block_bf(&last, size);
        if (!block) {
            
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            
            block->free = 0;
            if (block->size > size) {
                split_block(block, size);
            }
            total_free_size -= (block->size + sizeof(block_meta_t));
        }
    }
    return (block + 1);
}

void bf_free(void *ptr) {
    if (!ptr) return;

    block_meta_t *block_ptr = get_block_ptr(ptr);
    block_ptr->free = 1;

    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    coalesce();
}


unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}
