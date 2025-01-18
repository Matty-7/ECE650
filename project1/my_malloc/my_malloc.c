#include <unistd.h>
#include "my_malloc.h"

static block_meta_t *global_base = NULL; // the head of the linked list of memory blocks

static unsigned long total_size = 0;

static unsigned long total_free_size = 0;

// get access to the meta data of a memory block

block_meta_t *get_block_ptr(void *ptr) {
    return (block_meta_t*)(char*)ptr - sizeof(block_meta_t);
}

// find the appropriate free block for the allocation (first fit)
block_meta_t *find_free_block(block_meta_t **last, size_t size) {
    block_meta_t *current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

// request more memory from the system for the heap
block_meta_t *request_space(block_meta_t * last, size_t size) {
    block_meta_t * block;
    block = sbrk(0);
    void *request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*) -1) {
        return NULL;
    }
    if (last) {
        last->next = block;
    }
    block->size = size;
    block->next = NULL;
    block->free = 0;
    block->magic = 0x12345678;

    total_size += size + sizeof(block_meta_t);
    return block;
}

// first fit malloc
void *ff_malloc(size_t size) {
    if (size <= 0) return NULL;
    block_meta_t *block;

    if (global_base == NULL) {
        block = request_space(NULL, size);
        if (block == NULL) return NULL;
        global_base = block;
    } else {
        block_meta_t *last = global_base;
        block = find_free_block(&last, size);
        if (block != NULL) {
            block = request_space(last, size);
            if (block == NULL) return NULL;
        } else {
            block->free = 0;
            if (block->size >= size + sizeof(block_meta_t) + 8) {
                block_meta_t *new_block = (block_meta_t*)((char*)block + sizeof(block_meta_t) + size);
                new_block->size = block->size - size - sizeof(block_meta_t);
                new_block->free = 1;
                new_block->next = block->next;
                new_block->magic = 0x12345678;

                block->size = size;
                block->next = new_block;

                total_free_size += new_block->size;
            }
        }
    }

    return (block + 1);
}

// coalesce free blocks that are next to each other
void coalesce() {
    block_meta_t *current = global_base;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(block_meta_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// first fit free
void ff_free(void *ptr) {
    if (!ptr) return;

    block_meta_t *block_ptr = get_block_ptr(ptr);
    if (block_ptr->magic != 0x12345678) return; // check if the block is valid

    block_ptr->free = 1;
    total_free_size += block_ptr->size;

    coalesce();
}

unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

