#include "my_malloc.h"
#include <unistd.h>
#include <stdio.h>

Metadata *first_free_block = NULL;
Metadata *last_free_block = NULL;
Metadata *first_block = NULL;
size_t data_size = 0;
size_t free_size = 0;

typedef enum {
    FIRST_FIT,
    BEST_FIT
} AllocationStrategy;

void init_block(Metadata *block, size_t size, int isfree) {
    block->size = size;
    block->isfree = isfree;
    block->next = NULL;
    block->prev = NULL;
}

Metadata* first_fit(size_t requested_size) {
    Metadata *current = first_free_block;
    while (current != NULL) {
        if (current->size >= requested_size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

Metadata* best_fit(size_t requested_size) {
    Metadata *current = first_free_block;
    Metadata *best = NULL;
    while (current != NULL) {
        if (current->size >= requested_size) {
            if (best == NULL || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }
    return best;
}

void *generic_malloc(size_t requested_size, AllocationStrategy strategy) {
    Metadata *fit_block = NULL;
    if (strategy == FIRST_FIT) {
        fit_block = first_fit(requested_size);
    } else if (strategy == BEST_FIT) {
        fit_block = best_fit(requested_size);
    }

    if (fit_block != NULL) {
        return reuse_block(requested_size, fit_block);
    }

    return allocate_block(requested_size);
}

void *ff_malloc(size_t requested_size) {
    return generic_malloc(requested_size, FIRST_FIT);
}

void *bf_malloc(size_t requested_size) {
    return generic_malloc(requested_size, BEST_FIT);
}

void *reuse_block(size_t requested_size, Metadata *block) {
    size_t min_split_size = requested_size + sizeof(Metadata);
    if (block->size > min_split_size) {
        Metadata *remainder = (Metadata *)((char *)block + sizeof(Metadata) + requested_size);
        init_block(remainder, block->size - requested_size - sizeof(Metadata), 1);
        
        remove_block(block);
        add_block(remainder);
        block->size = requested_size;
        free_size -= (requested_size + sizeof(Metadata));
    } else {
        remove_block(block);
        free_size -= (block->size + sizeof(Metadata));
    }
    
    block->isfree = 0;
    block->next = NULL;
    block->prev = NULL;
    
    return (char *)block + sizeof(Metadata);
}

void *allocate_block(size_t requested_size) {
    size_t total_size = requested_size + sizeof(Metadata);
    void *memory = sbrk(total_size);
    if (memory == (void *)-1) {
        return NULL;
    }
    
    Metadata *new_block = (Metadata *)memory;
    init_block(new_block, requested_size, 0);
    
    data_size += total_size;
    if (first_block == NULL) {
        first_block = new_block;
    }
    
    return (char *)new_block + sizeof(Metadata);
}

void add_block(Metadata *block) {
    Metadata *current = first_free_block;
    Metadata *prev = NULL;

    while (current != NULL && block > current) {
        prev = current;
        current = current->next;
    }

    block->next = current;
    block->prev = prev;

    if (current != NULL) {
        current->prev = block;
    } else {
        last_free_block = block;
    }

    if (prev == NULL) {
        first_free_block = block;
    } else {
        prev->next = block;
    }
}

void remove_block(Metadata *block) {
    if (first_free_block == block && last_free_block == block) {
        first_free_block = NULL;
        last_free_block = NULL;
        return;
    }
    
    if (last_free_block == block) {
        last_free_block = block->prev;
        last_free_block->next = NULL;
        return;
    }
    
    if (first_free_block == block) {
        first_free_block = block->next;
        first_free_block->prev = NULL;
        return;
    }
    
    block->prev->next = block->next;
    block->next->prev = block->prev;
}

void ff_free(void *ptr) {
    Metadata *block = (Metadata *)((char *)ptr - sizeof(Metadata));
    
    block->isfree = 1;
    free_size += block->size + sizeof(Metadata);
    
    add_block(block);
    
    void *next_physical_addr = (char *)block + block->size + sizeof(Metadata);
    if (block->next != NULL && next_physical_addr == (char *)block->next) {
        block->size += sizeof(Metadata) + block->next->size;
        remove_block(block->next);
    }
    
    void *current_physical_addr = (char *)block;
    if (block->prev != NULL && 
        (char *)block->prev + block->prev->size + sizeof(Metadata) == current_physical_addr) {
        block->prev->size += sizeof(Metadata) + block->size;
        remove_block(block);
    }
}

void bf_free(void *ptr) {
    ff_free(ptr);
}

unsigned long get_data_segment_size() {
    return data_size;
}

unsigned long get_data_segment_free_space_size() {
    return free_size;
}

// ------ Helper functions ------
void print_free_list(void) {
    Metadata *current = first_free_block;
    printf("Free list:\n");
    while (current != NULL) {
        printf("Block at %p - Size: %zu\n", (void*)current, current->size);
        current = current->next;
    }
}

size_t count_free_blocks(void) {
    size_t count = 0;
    Metadata *current = first_free_block;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

int validate_heap(void) {
    Metadata *current = first_block;
    while (current != NULL) {
        if (current->size == 0) {
            return 0;
        }
        current = (Metadata *)((char *)current + sizeof(Metadata) + current->size);
    }
    return 1;
}

size_t get_allocation_count(void) {
    size_t count = 0;
    Metadata *current = first_block;
    while (current != NULL) {
        if (!current->isfree) {
            count++;
        }
        current = current->next;
    }
    return count;
}

void log_allocation(void* ptr, size_t size) {
    printf("Allocated %zu bytes at %p\n", size, ptr);
}

void free_all_blocks(void) {
    Metadata *current = first_block;
    while (current != NULL) {
        Metadata *next = current->next;
        current->isfree = 1;
        current->next = NULL;
        current->prev = NULL;
        add_block(current);
        current = next;
    }
    first_free_block = first_block;
    last_free_block = NULL;
    first_block = NULL;
    free_size = data_size;
}

Metadata* find_block(void* ptr) {
    Metadata *current = first_block;
    while (current != NULL) {
        if ((void *)current + sizeof(Metadata) == ptr) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void print_allocated_blocks(void) {
    Metadata *current = first_block;
    printf("Allocated Blocks:\n");
    while (current != NULL) {
        if (!current->isfree) {
            printf("Block at %p - Size: %zu bytes\n", (void*)current, current->size);
        }
        current = current->next;
    }
}

size_t get_largest_free_block(void) {
    Metadata *current = first_free_block;
    size_t largest = 0;
    while (current != NULL) {
        if (current->size > largest) {
            largest = current->size;
        }
        current = current->next;
    }
    return largest;
}

size_t get_total_allocated(void) {
    Metadata *current = first_block;
    size_t total = 0;
    while (current != NULL) {
        if (!current->isfree) {
            total += current->size;
        }
        current = (Metadata *)((char *)current + sizeof(Metadata) + current->size);
    }
    return total;
}


void print_heap_summary(void) {
    size_t total = get_data_segment_size();
    size_t free_space = get_data_segment_free_space_size();
    size_t allocated = get_total_allocated();
    size_t largest_free = get_largest_free_block();

    printf("Heap Summary:\n");
    printf("Total Size: %zu bytes\n", total);
    printf("Allocated: %zu bytes\n", allocated);
    printf("Free Space: %zu bytes\n", free_space);
    printf("Largest Free Block: %zu bytes\n", largest_free);
}
