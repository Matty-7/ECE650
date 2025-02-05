// my_malloc.c
#include "my_malloc.h"
#include <unistd.h>
#include <pthread.h>

Metadata *global_first_free = NULL;
Metadata *global_last_free = NULL;

__thread Metadata *local_first_free = NULL;
__thread Metadata *local_last_free = NULL;

size_t global_data_size = 0;
size_t global_free_size = 0;

pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_metadata(Metadata *block, size_t size, int is_free) {
    block->size   = size;
    block->isfree = is_free;
    block->next   = NULL;
    block->prev   = NULL;
}

static void ts_add_block(Metadata *block, Metadata **free_first, Metadata **free_last) {
    if (*free_first == NULL || block < *free_first) {
        block->prev = NULL;
        block->next = *free_first;
        if (*free_first)
            (*free_first)->prev = block;
        else
            *free_last = block;
        *free_first = block;
    } else {
        Metadata *curr = *free_first;
        while (curr->next && block > curr->next)
            curr = curr->next;
        block->prev = curr;
        block->next = curr->next;
        curr->next = block;
        if (block->next)
            block->next->prev = block;
        else
            *free_last = block;
    }
}

static void ts_remove_block(Metadata *block, Metadata **free_first, Metadata **free_last) {
    if (*free_first == block && *free_last == block) {
        *free_first = *free_last = NULL;
    } else if (*free_first == block) {
        *free_first = block->next;
        if (*free_first)
            (*free_first)->prev = NULL;
    } else if (*free_last == block) {
        *free_last = block->prev;
        if (*free_last)
            (*free_last)->next = NULL;
    } else {
        if (block->prev)
            block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
    block->next = block->prev = NULL;
}

static void *ts_reuse_block(size_t req_size, Metadata *block, Metadata **free_first, Metadata **free_last) {
    if (block->size > req_size + sizeof(Metadata)) {
        Metadata *split_block = (Metadata *)((char *)block + sizeof(Metadata) + req_size);
        init_metadata(split_block, block->size - req_size - sizeof(Metadata), 1);
        ts_remove_block(block, free_first, free_last);
        ts_add_block(split_block, free_first, free_last);
        global_free_size -= (req_size + sizeof(Metadata));
        block->size = req_size;
    } else {
        ts_remove_block(block, free_first, free_last);
        global_free_size -= (block->size + sizeof(Metadata));
    }
    block->isfree = 0;
    return (char *)block + sizeof(Metadata);
}

static void *ts_allocate_block(size_t req_size, int lock_sbrk) {
    size_t total_size = req_size + sizeof(Metadata);
    Metadata *new_block = NULL;
    if (lock_sbrk) {
        pthread_mutex_lock(&sbrk_mutex);
        new_block = sbrk(total_size);
        pthread_mutex_unlock(&sbrk_mutex);
    } else {
        new_block = sbrk(total_size);
    }
    if (new_block == (void *)-1)
        return NULL;
    init_metadata(new_block, req_size, 0);
    global_data_size += total_size;
    return (char *)new_block + sizeof(Metadata);
}

static void *ts_bf_malloc(size_t req_size, Metadata **free_first, Metadata **free_last, int lock_sbrk) {
    Metadata *curr = *free_first;
    Metadata *best_fit = NULL;
    while (curr) {
        if (curr->size >= req_size) {
            if (!best_fit || curr->size < best_fit->size)
                best_fit = curr;
            if (curr->size == req_size)
                break;
        }
        curr = curr->next;
    }
    if (best_fit)
        return ts_reuse_block(req_size, best_fit, free_first, free_last);
    return ts_allocate_block(req_size, lock_sbrk);
}

void *ts_malloc_lock(size_t size) {
    pthread_mutex_lock(&global_lock);
    void *ptr = ts_bf_malloc(size, &global_first_free, &global_last_free, 0);
    pthread_mutex_unlock(&global_lock);
    return ptr;
}

void ts_free_lock(void *ptr) {
    if (!ptr)
        return;
    pthread_mutex_lock(&global_lock);
    Metadata *block = (Metadata *)((char *)ptr - sizeof(Metadata));
    block->isfree = 1;
    global_free_size += block->size + sizeof(Metadata);
    ts_add_block(block, &global_first_free, &global_last_free);
    if (block->next && ((char *)block + block->size + sizeof(Metadata) == (char *)block->next)) {
        block->size += sizeof(Metadata) + block->next->size;
        ts_remove_block(block->next, &global_first_free, &global_last_free);
    }
    if (block->prev && ((char *)block->prev + block->prev->size + sizeof(Metadata) == (char *)block)) {
        block->prev->size += sizeof(Metadata) + block->size;
        ts_remove_block(block, &global_first_free, &global_last_free);
    }
    pthread_mutex_unlock(&global_lock);
}

void *ts_malloc_nolock(size_t size) {
    return ts_bf_malloc(size, &local_first_free, &local_last_free, 1);
}

void ts_free_nolock(void *ptr) {
    if (!ptr)
        return;
    Metadata *block = (Metadata *)((char *)ptr - sizeof(Metadata));
    block->isfree = 1;
    global_free_size += block->size + sizeof(Metadata);
    ts_add_block(block, &local_first_free, &local_last_free);
    if (block->next && ((char *)block + block->size + sizeof(Metadata) == (char *)block->next)) {
        block->size += sizeof(Metadata) + block->next->size;
        ts_remove_block(block->next, &local_first_free, &local_last_free);
    }
    if (block->prev && ((char *)block->prev + block->prev->size + sizeof(Metadata) == (char *)block)) {
        block->prev->size += sizeof(Metadata) + block->size;
        ts_remove_block(block, &local_first_free, &local_last_free);
    }
}

unsigned long get_data_segment_size() {
    return global_data_size;
}

unsigned long get_data_segment_free_space_size() {
    return global_free_size;
}
