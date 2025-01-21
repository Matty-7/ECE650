#include "my_malloc.h"
#include <unistd.h>
#include <assert.h>

// Global variables for free list and heap management
static Block* free_list_head = NULL;
static Block* heap_tail = NULL;
static unsigned long total_data_segment_size = 0;
static long long total_free_space_size = 0;

// First Fit malloc implementation
void* ff_malloc(size_t size) {
    return internal_malloc(size, find_first_fit);
}

// First Fit free implementation
void ff_free(void* ptr) {
    internal_free(ptr);
}

// Best Fit malloc implementation
void* bf_malloc(size_t size) {
    return internal_malloc(size, find_best_fit);
}

// Best Fit free implementation
void bf_free(void* ptr) {
    internal_free(ptr);
}

// Returns the total size of the data segment
unsigned long get_data_segment_size() {
    return total_data_segment_size;
}

// Returns the total free space in the data segment
long long get_data_segment_free_space_size() {
    return total_free_space_size;
}

// Internal malloc function that uses a specified block finding strategy
void* internal_malloc(size_t size, find_block_func_t find_block) {
    if (size == 0) {
        return NULL;
    }

    Block* suitable_block = find_block(size);

    if (suitable_block) {
        if (suitable_block->size >= size + sizeof(Block) + 1) { // Ensure there's space to split
            split_block(suitable_block, size);
        }
        remove_from_free_list(suitable_block);
        suitable_block->is_free = 0;
        total_free_space_size -= (suitable_block->size + sizeof(Block));
        return (void*)(suitable_block + 1);
    }

    // No suitable block found; request more memory from the heap
    Block* new_block = request_memory_from_heap(size);
    if (!new_block) {
        return NULL;
    }
    return (void*)(new_block + 1);
}

// Internal free function
void internal_free(void* ptr) {
    if (!ptr) {
        return;
    }

    Block* block = ((Block*)ptr) - 1;
    block->is_free = 1;
    add_to_free_list(block);
    total_free_space_size += (block->size + sizeof(Block));
    merge_blocks(block);
}

// Find the first free block that fits the requested size
Block* find_first_fit(size_t size) {
    Block* current = free_list_head;
    while (current) {
        if (current->size >= size) {
            return current;
        }
        current = current->next_free;
    }
    return NULL;
}

// Find the best free block that fits the requested size
Block* find_best_fit(size_t size) {
    Block* current = free_list_head;
    Block* best = NULL;
    while (current) {
        if (current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
                if (best->size == size) { // Perfect fit
                    break;
                }
            }
        }
        current = current->next_free;
    }
    return best;
}

// Request memory from the heap using sbrk
Block* request_memory_from_heap(size_t size) {
    size_t total_size = size + sizeof(Block);
    void* ptr = sbrk(total_size);
    if (ptr == (void*)-1) {
        return NULL;
    }

    total_data_segment_size += total_size;

    Block* new_block = (Block*)ptr;
    new_block->size = size;
    new_block->is_free = 0;
    new_block->next_phys = NULL;
    new_block->prev_phys = heap_tail;
    new_block->next_free = NULL;
    new_block->prev_free = NULL;

    if (heap_tail) {
        heap_tail->next_phys = new_block;
    }
    heap_tail = new_block;

    return new_block;
}

// Add a block to the front of the free list
void add_to_free_list(Block* block) {
    block->next_free = free_list_head;
    block->prev_free = NULL;
    if (free_list_head) {
        free_list_head->prev_free = block;
    }
    free_list_head = block;
}

// Remove a block from the free list
void remove_from_free_list(Block* block) {
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        free_list_head = block->next_free;
    }

    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }

    block->next_free = NULL;
    block->prev_free = NULL;
}

// Merge adjacent free blocks
void merge_blocks(Block* block) {
    // Merge with next physical block if it's free
    if (block->next_phys && block->next_phys->is_free) {
        Block* next_block = block->next_phys;
        remove_from_free_list(next_block);
        block->size += sizeof(Block) + next_block->size;
        block->next_phys = next_block->next_phys;
        if (next_block->next_phys) {
            next_block->next_phys->prev_phys = block;
        } else {
            heap_tail = block;
        }
    }

    // Merge with previous physical block if it's free
    if (block->prev_phys && block->prev_phys->is_free) {
        Block* prev_block = block->prev_phys;
        remove_from_free_list(prev_block);
        prev_block->size += sizeof(Block) + block->size;
        prev_block->next_phys = block->next_phys;
        if (block->next_phys) {
            block->next_phys->prev_phys = prev_block;
        } else {
            heap_tail = prev_block;
        }
        block = prev_block;
    }
}

// Split a block into two if it's significantly larger than needed
Block* split_block(Block* block, size_t size) {
    if (block->size < size + sizeof(Block) + 1) {
        return block;
    }

    Block* new_block = (Block*)((char*)block + sizeof(Block) + size);
    new_block->size = block->size - size - sizeof(Block);
    new_block->is_free = 1;
    new_block->next_phys = block->next_phys;
    new_block->prev_phys = block;
    new_block->next_free = NULL;
    new_block->prev_free = NULL;

    if (block->next_phys) {
        block->next_phys->prev_phys = new_block;
    } else {
        heap_tail = new_block;
    }
    block->next_phys = new_block;
    block->size = size;

    add_to_free_list(new_block);
    total_free_space_size += (new_block->size + sizeof(Block));

    return new_block;
}
