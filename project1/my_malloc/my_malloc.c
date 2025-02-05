#include "my_malloc.h"
#include <unistd.h>

Metadata *first_free_block = NULL;
Metadata *last_free_block = NULL;
Metadata *first_block = NULL;
size_t data_size = 0;
size_t free_size = 0;

static void init_block(Metadata *block, size_t size, int isfree) {
    block->size = size;
    block->isfree = isfree;
    block->next = NULL;
    block->prev = NULL;
}

void *ff_malloc(size_t requested_size) {
    if (first_free_block != NULL) {
        Metadata *current_block = first_free_block;
        
        while (current_block != NULL) {
            if (current_block->size >= requested_size) {
                return reuse_block(requested_size, current_block);
            }
            current_block = current_block->next;
        }
    }
    
    return allocate_block(requested_size);
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
    if (first_free_block == NULL || block < first_free_block) {
        block->prev = NULL;
        block->next = first_free_block;
        
        if (first_free_block != NULL) {
            first_free_block->prev = block;
        } else {
            last_free_block = block;
        }
        first_free_block = block;
        return;
    }
    
    Metadata *current = first_free_block;
    
    while (current->next != NULL && block > current->next) {
        current = current->next;
    }
    
    block->prev = current;
    block->next = current->next;
    current->next = block;
    
    if (block->next != NULL) {
        block->next->prev = block;
    } else {
        last_free_block = block;
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

void *bf_malloc(size_t size) {
    Metadata *current = first_free_block;
    Metadata *best_fit = NULL;
    
    while (current != NULL) {
        if (current->size >= size) {
            if (best_fit == NULL || current->size < best_fit->size) {
                best_fit = current;
            }
            if (current->size == size) {
                break;
            }
        }
        current = current->next;
    }
    
    if (best_fit != NULL) {
        return reuse_block(size, best_fit);
    }
    return allocate_block(size);
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
