#include "my_malloc.h"
#include <unistd.h>
#include <assert.h>

static mem_block_t *free_list_head = NULL;
static mem_block_t *heap_end_block = NULL;

static unsigned long total_data_segment_bytes = 0;
static long long total_free_space_bytes = 0;

void *ff_malloc(size_t size) {
    return allocate_memory(size, locate_first_fit);
}

void ff_free(void *ptr) {
    release_memory(ptr);
}

void *bf_malloc(size_t size) {
    return allocate_memory(size, locate_best_fit);
}

void bf_free(void *ptr) {
    release_memory(ptr);
}

unsigned long get_data_segment_size() {
    return total_data_segment_bytes;
}

long long get_data_segment_free_space_size() {
    return total_free_space_bytes;
}

static void *allocate_memory(size_t size, finder_func_t strategy) {
    if (size == 0) {
        return NULL;
    }

    // Look for a free block
    mem_block_t *selected_block = strategy(size);

    // If not found, request fresh memory from the OS (sbrk)
    if (!selected_block) {
        selected_block = grow_heap(size);
        if (!selected_block) {
            return NULL;  // sbrk failed
        }
    } else {
        detach_from_free_list(selected_block);

        // If it’s large enough to split off a chunk for future use, we should split it
        if (selected_block->block_size >= size + sizeof(mem_block_t) + 1) {
            divide_block(selected_block, size);
        }
        // Adjust free space size
        total_free_space_bytes -= (selected_block->block_size + sizeof(mem_block_t));
    }

    // Mark as allocated and return the payload area
    selected_block->is_free = 0;
    return (void *)(selected_block + 1);
}

static void release_memory(void *ptr) {
    if (!ptr) {
        return;
    }
    mem_block_t *block = (mem_block_t *)ptr - 1;
    block->is_free = 1;
    total_free_space_bytes += (block->block_size + sizeof(mem_block_t));
    attach_to_free_list(block);
    merge_with_neighbors(block);
}

static mem_block_t *locate_first_fit(size_t size) {
    mem_block_t *current = free_list_head;
    while (current) {
        if (current->block_size >= size) {
            return current;
        }
        current = current->free_next;
    }
    return NULL;
}

static mem_block_t *locate_best_fit(size_t size) {
    mem_block_t *candidate = NULL;
    for (mem_block_t *c = free_list_head; c; c = c->free_next) {
        if (c->block_size >= size) {
            if (!candidate || c->block_size < candidate->block_size) {
                candidate = c;
                if (c->block_size == size) {
                    break;  // perfect match
                }
            }
        }
    }
    return candidate;
}

static mem_block_t *grow_heap(size_t size) {
    size_t actual_size = size + sizeof(mem_block_t);
    void *new_space = sbrk(actual_size);
    if (new_space == (void *)-1) {
        return NULL;
    }

    total_data_segment_bytes += actual_size;

    // Build the new block at the start of the new space
    mem_block_t *block = (mem_block_t *)new_space;
    block->block_size = size;
    block->is_free = 0;
    block->phys_next = NULL;
    block->phys_prev = heap_end_block;
    block->free_next = NULL;
    block->free_prev = NULL;

    // Chain in physically
    if (heap_end_block) {
        heap_end_block->phys_next = block;
    }
    heap_end_block = block;

    return block;
}

static void attach_to_free_list(mem_block_t *block) {
    block->free_next = free_list_head;
    block->free_prev = NULL;
    if (free_list_head) {
        free_list_head->free_prev = block;
    }
    free_list_head = block;
}

static void detach_from_free_list(mem_block_t *block) {
    if (!block) return;

    mem_block_t *prev_free = block->free_prev;
    mem_block_t *next_free = block->free_next;

    if (prev_free) {
        prev_free->free_next = next_free;
    } else {
        free_list_head = next_free;
    }
    if (next_free) {
        next_free->free_prev = prev_free;
    }

    block->free_prev = NULL;
    block->free_next = NULL;
}

static void merge_with_neighbors(mem_block_t *block) {
    // Merge forward if the next physical neighbor is free.
    if (block->phys_next && block->phys_next->is_free) {
        mem_block_t *next_b = block->phys_next;
        detach_from_free_list(next_b);
        block->block_size += sizeof(mem_block_t) + next_b->block_size;
        block->phys_next = next_b->phys_next;
        if (block->phys_next) {
            block->phys_next->phys_prev = block;
        } else {
            heap_end_block = block;
        }
    }

    // Merge backward if the previous physical neighbor is free.
    if (block->phys_prev && block->phys_prev->is_free) {
        mem_block_t *prev_b = block->phys_prev;
        detach_from_free_list(prev_b);
        prev_b->block_size += sizeof(mem_block_t) + block->block_size;
        prev_b->phys_next = block->phys_next;
        if (block->phys_next) {
            block->phys_next->phys_prev = prev_b;
        } else {
            heap_end_block = prev_b;
        }
        block = prev_b; // block is now merged into prev_b
    }
}

static void divide_block(mem_block_t *block, size_t requested) {
    // Where the new block’s metadata starts
    char *split_addr = (char *)block + sizeof(*block) + requested;
    mem_block_t *new_block = (mem_block_t *)split_addr;

    new_block->block_size = block->block_size - requested - sizeof(mem_block_t);
    new_block->is_free = 1;
    new_block->phys_prev = block;
    new_block->phys_next = block->phys_next;

    if (block->phys_next) {
        block->phys_next->phys_prev = new_block;
    } else {
        heap_end_block = new_block;
    }

    block->block_size = requested;
    block->phys_next = new_block;

    // Put the new block in the free list
    new_block->free_next = NULL;
    new_block->free_prev = NULL;
    attach_to_free_list(new_block);

    // Increase free space usage for newly created block
    total_free_space_bytes += (new_block->block_size + sizeof(mem_block_t));
}
