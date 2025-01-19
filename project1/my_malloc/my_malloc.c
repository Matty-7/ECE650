#include "my_malloc.h"
#include <unistd.h>    // for sbrk
#include <stdint.h>    // for uintptr_t if needed
#include <stddef.h>    // for NULL, size_t

// ------------------
// 全局变量
// ------------------
static block_meta_t *global_base = NULL;  // 链表头指针
static block_meta_t *global_last = NULL;  // 链表尾指针（便于在 sbrk 后面追加块）

// 以下两个变量用来统计总堆大小和空闲空间大小（包括元数据的大小）
static unsigned long total_size = 0;
static unsigned long total_free_size = 0;

// 帮助函数：给定用户指针 ptr，返回其对应的 block_meta_t*
static block_meta_t *get_block_ptr(void *ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}

// ------------------
// 局部函数声明
// ------------------
static block_meta_t* request_space(block_meta_t* last, size_t size);
static void split_block(block_meta_t* block, size_t size);
static void coalesce_block(block_meta_t* block);

// first fit 找空闲块
static block_meta_t* find_free_block_ff(size_t size);
// best fit 找空闲块
static block_meta_t* find_free_block_bf(size_t size);

// ------------------
// 工具函数：获取堆与空闲大小
// ------------------
unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

// ------------------
// 分配策略辅助函数
// ------------------

static block_meta_t *last_alloc = NULL; // 记录上一次分配搜索到的块位置

static block_meta_t* find_free_block_ff(size_t size) {
    // 先从 last_alloc 开始往后找
    block_meta_t* current = last_alloc ? last_alloc : global_base;
    // 如果链表是单向的，要注意别陷入死循环，可先记录一下起点
    block_meta_t* start = current;

    // (1) 从 last_alloc 往下找
    while (current) {
        if (current->free && current->size >= size) {
            last_alloc = current;  // 找到后更新
            return current;
        }
        current = current->next;
    }
    // (2) 如果没找到，就从表头开始找一遍
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

// 按 Best Fit 在链表中找最优空闲块
static block_meta_t* find_free_block_bf(size_t size) {
    block_meta_t *current = global_base;
    block_meta_t *best_fit = NULL;
    size_t smallest_diff = (size_t)-1;  // 或者 SIZE_MAX

    while (current) {
        if (current->free && current->size >= size) {
            size_t diff = current->size - size;
            if (diff < smallest_diff) {
                smallest_diff = diff;
                best_fit = current;
                if (diff == 0) {
                    // 完美匹配，直接返回
                    return best_fit;
                }
            }
        }
        current = current->next;
    }
    return best_fit;
}

// 如果没有可用的空闲块（或空闲块不够容纳新申请），向系统申请空间
static block_meta_t* request_space(block_meta_t* last, size_t size) {
    // sbrk(0) 给出当前 break 的地址，作为新块的起始地址
    block_meta_t* block = sbrk(0);
    void *request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*)-1) {
        // sbrk 失败
        return NULL;
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;
    block->prev = last;

    // 如果之前的 last 不为空，说明链表原来有节点，需要更新 last->next
    if (last) {
        last->next = block;
    }
    // 如果全局的 global_base 还没有初始化，需要更新
    if (!global_base) {
        global_base = block;
    }
    // 更新 global_last
    global_last = block;

    // 更新堆大小
    total_size += (size + sizeof(block_meta_t));

    return block;
}

// 分裂大块：如果当前块的 size 比需要的 size 大太多，就拆分出一个新 free 块
static void split_block(block_meta_t* block, size_t size) {
    // 判断是否能放得下新的 block_meta_t + 至少1个字节的剩余空间
    if (block->size >= size + sizeof(block_meta_t) + 1) {
        // 新块的起始地址：在当前块中向后偏移 size + 元数据
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

        // 原块缩小
        block->size = size;

        // 更新 total_free_size：新块是 free 的
        total_free_size += (new_block->size + sizeof(block_meta_t));
    }
}

// 就地合并：只合并“我”这个块和它的相邻块，不要再全表扫描
static void coalesce_block(block_meta_t* block) {
    // ----------------------
    // 与后面的相邻空闲块合并
    // ----------------------
    while (block->next && block->next->free) {
        block_meta_t* next_block = block->next;
        // 把 next_block 合并进当前 block
        block->size += (next_block->size + sizeof(block_meta_t));

        // 调整 free 空间计数，metadata 被合并了，所以要减去一个 block_meta_t 的大小
        total_free_size -= sizeof(block_meta_t);

        // 从链表中摘除 next_block
        block->next = next_block->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            // 若没有下一个块，说明 block 已是尾巴，更新 global_last
            global_last = block;
        }
    }

    // ----------------------
    // 与前面的相邻空闲块合并
    // ----------------------
    while (block->prev && block->prev->free) {
        block_meta_t* prev_block = block->prev;
        prev_block->size += (block->size + sizeof(block_meta_t));

        // 同理，合并后更新 free 空间计数
        total_free_size -= sizeof(block_meta_t);

        // 从链表中摘除 block
        prev_block->next = block->next;
        if (block->next) {
            block->next->prev = prev_block;
        } else {
            global_last = prev_block;
        }
        block = prev_block; 
        // 注意，这里 block 指针改了，以便可能继续往前再合并
    }
}

// ------------------
// First Fit
// ------------------
void* ff_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // 先在链表中找空闲块
    block_meta_t* block = find_free_block_ff(size);
    if (!block) {
        // 没找到可用的块，向系统申请
        block = request_space(global_last, size);
        if (!block) {
            // sbrk 失败
            return NULL;
        }
    } else {
        // 找到了合适的空闲块
        block->free = 0;
        // 分裂
        // 注意：先从 total_free_size 中把这个 block 的整块都减去
        // 之后若分裂出新的块，再把新块加回去
        total_free_size -= (block->size + sizeof(block_meta_t));
        split_block(block, size);
    }

    // 最终返回可用数据区的指针（跳过元数据）
    return (char*)block + sizeof(block_meta_t);
}

void ff_free(void *ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block_ptr = get_block_ptr(ptr);
    block_ptr->free = 1;

    // 更新 free 空间
    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    // 只合并当前块与它的前后邻居
    coalesce_block(block_ptr);
}

// ------------------
// Best Fit
// ------------------
void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    block_meta_t* block = find_free_block_bf(size);
    if (!block) {
        // 没找到可用的块，向系统申请
        block = request_space(global_last, size);
        if (!block) {
            return NULL;
        }
    } else {
        // 找到了合适的空闲块
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
