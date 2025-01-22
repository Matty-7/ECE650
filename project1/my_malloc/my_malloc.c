#include "my_malloc.h"
#include <assert.h>

typedef struct mem_block {
    size_t size;                    
    int is_free;                    
    struct mem_block *prev_physical; 
    struct mem_block *next_physical; 
    struct mem_block *prev_free;     
    struct mem_block *next_free;     
} mem_block_t;

typedef mem_block_t *(*search_func_t)(size_t size);

static mem_block_t *g_free_list_head = NULL;

static mem_block_t *g_physical_list_tail = NULL;

static unsigned long g_data_segment_size = 0;

static unsigned long g_data_segment_free_space_size = 0;

static void *internal_malloc(size_t size, search_func_t search_func);
static void internal_free(void *ptr);

static mem_block_t *find_first_fit(size_t size);
static mem_block_t *find_best_fit(size_t size);

static mem_block_t *request_new_memory_block(size_t size);
static mem_block_t *split_block(mem_block_t *block, size_t size);
static void merge_physical_next_block(mem_block_t *block);

static void add_free_block_front(mem_block_t *block);
static void remove_free_block(mem_block_t *block);
static void merge_free_block(mem_block_t *block);

void *ff_malloc(size_t size) {
    return internal_malloc(size, find_first_fit);
}

void ff_free(void *ptr) {
    internal_free(ptr);
}

void *bf_malloc(size_t size) {
    return internal_malloc(size, find_best_fit);
}

void bf_free(void *ptr) {
    internal_free(ptr);
}

unsigned long get_data_segment_size() {
    return g_data_segment_size;
}

unsigned long get_data_segment_free_space_size() {
    return g_data_segment_free_space_size;
}

static void *internal_malloc(size_t size, search_func_t search_func) {
    if (size == 0) {
        return NULL;
    }

    if (!g_free_list_head) {
        mem_block_t *new_block = request_new_memory_block(size);
        if (new_block == NULL) {
            return NULL;
        }
        return (void *)(new_block + 1);
    } else {
        mem_block_t *candidate = search_func(size);
        if (!candidate) {
            mem_block_t *new_block = request_new_memory_block(size);
            if (new_block == NULL) {
                return NULL;
            } else {
                return (void *)(new_block + 1);
            }
        } else if ((*candidate).size <= size + sizeof(mem_block_t)) {
            g_data_segment_free_space_size -= ((*candidate).size + sizeof(mem_block_t));
            remove_free_block(candidate);
            (*candidate).is_free = 0;
            return (void *)(candidate + 1);
        } else {
            g_data_segment_free_space_size -= (size + sizeof(mem_block_t));
            mem_block_t *new_block = split_block(candidate, size);
            return (void *)(new_block + 1);
        }
    }
}

static void internal_free(void *ptr) {
    if (ptr == NULL) return;

    mem_block_t *block = (mem_block_t *)ptr - 1;
    if (!(*block).is_free) {
        add_free_block_front(block);
        merge_free_block(block);
    }
}

static mem_block_t *find_first_fit(size_t size) {
    mem_block_t *curr = g_free_list_head;
    while (curr) {
        if ((*curr).size >= size) {
            return curr;
        }
        curr = (*curr).next_free;
    }
    return NULL;
}

static mem_block_t *find_best_fit(size_t size) {
    mem_block_t *best = NULL;
    mem_block_t *curr = g_free_list_head;
    while (curr) {
        if ((*curr).size >= size) {
            if (!best || (*curr).size < (*best).size) {
                best = curr;
            }
            if ((*curr).size == size) {
                return curr;
            }
        }
        curr = (*curr).next_free;
    }
    return best;
}

static mem_block_t *request_new_memory_block(size_t size) {
    size_t bytes_to_allocate;
    if (size + 2 * sizeof(mem_block_t) > (3 * sysconf(_SC_PAGESIZE))) {
        bytes_to_allocate = size + sizeof(mem_block_t);
    } else {
        bytes_to_allocate = 3 * sysconf(_SC_PAGESIZE);
    }

    void *ptr = sbrk(bytes_to_allocate);
    if (ptr == (void *)-1) {
        return NULL;
    }

    g_data_segment_size += bytes_to_allocate;

    mem_block_t *new_block = (mem_block_t *)ptr;
    *new_block = (mem_block_t){
        .size = size,
        .is_free = 0,
        .prev_free = NULL,
        .next_free = NULL,
        .prev_physical = g_physical_list_tail,
        .next_physical = NULL
    };

    if (g_physical_list_tail) {
        (*g_physical_list_tail).next_physical = new_block;
    }
    g_physical_list_tail = new_block;

    if (bytes_to_allocate == (3 * sysconf(_SC_PAGESIZE))) {
        size_t leftover = bytes_to_allocate - (sizeof(mem_block_t) + size);
        if (leftover > sizeof(mem_block_t)) {
            mem_block_t *free_block = (mem_block_t *)((char *)ptr
                                      + sizeof(mem_block_t) + size);
            mem_block_t block = {
                .size = leftover - sizeof(mem_block_t),
                .is_free = 0,
                .prev_free = NULL,
                .next_free = NULL,
                .prev_physical = new_block,
                .next_physical = NULL
            };
            *free_block = block;

            (*new_block).next_physical = free_block;
            g_physical_list_tail = free_block;

            add_free_block_front(free_block);
        }
    }
    return new_block;
}

static mem_block_t *split_block(mem_block_t *block, size_t size) {
    size_t leftover = block->size - (size + sizeof(mem_block_t));
    mem_block_t *new_block = (mem_block_t *)((char *)block
                               + sizeof(mem_block_t)
                               + leftover);

    *new_block = (mem_block_t){
        .size = size,
        .is_free = 0,
        .prev_physical = block,
        .next_physical = (*block).next_physical,
        .prev_free = NULL,
        .next_free = NULL
    };

    block->size = leftover;
    if ((*new_block).next_physical) {
        (*(*new_block).next_physical).prev_physical = new_block;
    } else {
        g_physical_list_tail = new_block;
    }
    (*block).next_physical = new_block;

    return new_block;
}

static void merge_physical_next_block(mem_block_t *block) {
    mem_block_t *next_block = (*block).next_physical;
    if (!next_block || !(*next_block).is_free) {
        return;
    }
    (*block).size += sizeof(mem_block_t) + (*next_block).size;
    (*block).next_physical = (*next_block).next_physical;
    if ((*block).next_physical) {
        (*(*block).next_physical).prev_physical = block;
    } else {
        g_physical_list_tail = block;
    }
}

static void add_free_block_front(mem_block_t *block) {
    if (!(*block).is_free) {
        
        g_data_segment_free_space_size += (block->size + sizeof(mem_block_t));
        (*block).is_free = 1;
    }
    (*block).prev_free = NULL;
    (*block).next_free = g_free_list_head;
    if (g_free_list_head) {
        (*g_free_list_head).prev_free = block;
    }
    g_free_list_head = block;
}

static void remove_free_block(mem_block_t *block) {
    mem_block_t *p = (*block).prev_free;
    mem_block_t *n = (*block).next_free;

    if (p) {
        (*p).next_free = n;
    } else {
        g_free_list_head = n;
    }
    if (n) {
        (*n).prev_free = p;
    }

    (*block).prev_free = NULL;
    (*block).next_free = NULL;
}

static void merge_free_block(mem_block_t *block) {
    if (!block || !(*block).is_free) {
        return;
    }

    mem_block_t *next_block = (*block).next_physical;
    if (next_block && (*next_block).is_free) {
        remove_free_block(next_block);
        (*block).size += sizeof(mem_block_t) + (*next_block).size;
        (*block).next_physical = (*next_block).next_physical;
        if ((*block).next_physical) {
            (*(*block).next_physical).prev_physical = block;
        } else {
            g_physical_list_tail = block;
        }
    }

    mem_block_t *prev_block = (*block).prev_physical;
    if (prev_block && (*prev_block).is_free) {
        
        remove_free_block(block);
        
        (*prev_block).size += sizeof(mem_block_t) + (*block).size;
        (*prev_block).next_physical = (*block).next_physical;
        if ((*prev_block).next_physical) {
            (*(*prev_block).next_physical).prev_physical = prev_block;
        } else {
            g_physical_list_tail = prev_block;
        }
    }
}
