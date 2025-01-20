#include "my_malloc.h"
#include <unistd.h>     // for sbrk
#include <stdint.h>
#include <stddef.h>     // for NULL, size_t

// A basic AVL tree storing free blocks by size, plus one fast bin for blocks
// of size EXACTLY 128 bytes. The fast bin is just a singly linked list.

typedef struct tree_node {
    struct tree_node* left;
    struct tree_node* right;
    int height;
    size_t size;
    block_meta_t* block;
} tree_node_t;

static tree_node_t* free_root = NULL;

// A singly linked list of freed 128-byte blocks
static block_meta_t* fastbin_128 = NULL;

static unsigned long total_size = 0;      // includes metadata
static unsigned long total_free_size = 0; // including metadata

static const size_t FASTBIN_SIZE = 128;   

static size_t align8(size_t size);
static block_meta_t* request_space(size_t size);
static void split_block(block_meta_t* block, size_t size);
static void coalesce_block(block_meta_t* block);

static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block);
static tree_node_t* avl_delete(tree_node_t* node, size_t size, block_meta_t* block);
static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size);
static int height(tree_node_t* node);
static int get_balance(tree_node_t* node);
static void update_height(tree_node_t* node);
static tree_node_t* left_rotate(tree_node_t* x);
static tree_node_t* right_rotate(tree_node_t* y);

static void insert_free_block(block_meta_t* block);
static void remove_free_block(block_meta_t* block);

static block_meta_t* get_block_ptr(void* ptr);

unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

static size_t align8(size_t size) {
    return (size + 7) & ~((size_t)7);
}

static block_meta_t* get_block_ptr(void* ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}

// AVL utilities

static int height(tree_node_t* node) {
    return node ? node->height : 0;
}

static int get_balance(tree_node_t* node) {
    return node ? (height(node->left) - height(node->right)) : 0;
}

static void update_height(tree_node_t* node) {
    int hl = height(node->left);
    int hr = height(node->right);
    node->height = (hl > hr ? hl : hr) + 1;
}

static tree_node_t* right_rotate(tree_node_t* y) {
    tree_node_t* x = y->left;
    tree_node_t* T2 = x->right;

    x->right = y;
    y->left = T2;

    update_height(y);
    update_height(x);
    return x;
}

static tree_node_t* left_rotate(tree_node_t* x) {
    tree_node_t* y = x->right;
    tree_node_t* T2 = y->left;

    y->left = x;
    x->right = T2;

    update_height(x);
    update_height(y);
    return y;
}

static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block) {
    if (!node) {
        // directly sbrk space to store tree nodes themselves.
        tree_node_t* new_node = (tree_node_t*)sbrk(sizeof(tree_node_t));
        if (new_node == (void*)-1) {
            return NULL; // sbrk failed
        }
        new_node->left = new_node->right = NULL;
        new_node->height = 1;
        new_node->size = block->size;
        new_node->block = block;
        total_size += sizeof(tree_node_t);
        return new_node;
    }

    if (block->size < node->size) {
        node->left = avl_insert(node->left, block);
    } else {
        node->right = avl_insert(node->right, block);
    }

    update_height(node);

    int balance = get_balance(node);

    // Left Left
    if (balance > 1 && block->size < node->left->size) {
        return right_rotate(node);
    }
    // Right Right
    if (balance < -1 && block->size >= node->right->size) {
        return left_rotate(node);
    }
    // Left Right
    if (balance > 1 && block->size >= node->left->size) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
    // Right Left
    if (balance < -1 && block->size < node->right->size) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

static tree_node_t* min_value_node(tree_node_t* node) {
    tree_node_t* current = node;
    while (current && current->left) {
        current = current->left;
    }
    return current;
}

static tree_node_t* avl_delete(tree_node_t* node, size_t size, block_meta_t* block) {
    if (!node) {
        return NULL;
    }
    if (size < node->size) {
        node->left = avl_delete(node->left, size, block);
    } else if (size > node->size) {
        node->right = avl_delete(node->right, size, block);
    } else {
        // size == node->size
        if (node->block == block) {
            // Found the node
            if (!node->left && !node->right) {
                node = NULL;
            } else if (!node->left) {
                node = node->right;
            } else if (!node->right) {
                node = node->left;
            } else {
                // two children
                tree_node_t* temp = min_value_node(node->right);
                node->size = temp->size;
                node->block = temp->block;
                node->right = avl_delete(node->right, temp->size, temp->block);
            }
            return node;
        } else {
            // same size, different pointer
            node->right = avl_delete(node->right, size, block);
        }
    }

    if (!node) {
        return NULL;
    }

    update_height(node);
    int balance = get_balance(node);

    // re-balance
    if (balance > 1 && get_balance(node->left) >= 0) {
        return right_rotate(node);
    }
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
    if (balance < -1 && get_balance(node->right) <= 0) {
        return left_rotate(node);
    }
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size) {
    tree_node_t* best = NULL;
    tree_node_t* current = root;

    while (current) {
        if (current->size == size) {
            best = current;
            break;
        } else if (current->size > size) {
            best = current;
            current = current->left;
        } else {
            // current->size < size
            current = current->right;
        }
    }
    return best;
}

static void insert_free_block(block_meta_t* block) {
    free_root = avl_insert(free_root, block);
}

static void remove_free_block(block_meta_t* block) {
    free_root = avl_delete(free_root, block->size, block);
}

// 128-byte blocks

static void fastbin_push_128(block_meta_t* block) {
    block->next = fastbin_128;
    fastbin_128 = block;
}

static block_meta_t* fastbin_pop_128() {
    if (!fastbin_128) {
        return NULL;
    }
    block_meta_t* block = fastbin_128;
    fastbin_128 = block->next;
    block->next = NULL;
    return block;
}

// via sbrk

static block_meta_t* request_space(size_t size) {
    block_meta_t* block = sbrk(0);
    void* request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*)-1) {
        return NULL;
    }
    block->size = size;
    block->free = 0;
    block->prev = NULL;
    block->next = NULL;

    total_size += (size + sizeof(block_meta_t));
    return block;
}

static void split_block(block_meta_t* block, size_t size) {
    if (block->size >= size + sizeof(block_meta_t) + 1) {
        char* address = (char*)block;
        block_meta_t* new_block = (block_meta_t*)(address + sizeof(block_meta_t) + size);

        new_block->size = block->size - size - sizeof(block_meta_t);
        new_block->free = 1;
        new_block->prev = NULL;
        new_block->next = NULL;

        block->size = size;

        insert_free_block(new_block);
        total_free_size += (new_block->size + sizeof(block_meta_t));
    }
}

static void coalesce_block(block_meta_t* block) {
    
}

void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = align8(size);

    // If size matches our fast bin size, pop if available
    if (size == FASTBIN_SIZE) {
        block_meta_t* fast_block = fastbin_pop_128();
        if (fast_block) {
            fast_block->free = 0;
            total_free_size -= (fast_block->size + sizeof(block_meta_t));
            return (char*)fast_block + sizeof(block_meta_t);
        }
    }

    // Otherwise proceed with best-fit in the AVL tree
    tree_node_t* best_node = find_best_fit_node(free_root, size);
    block_meta_t* block = NULL;

    if (!best_node) {
        // no suitable block
        block = request_space(size);
        if (!block) {
            return NULL;
        }
    } else {
        block = best_node->block;
        remove_free_block(block);
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
    if (block_ptr->free) {
        // ignoring double-free
        return;
    }
    block_ptr->free = 1;

    // If it matches our fast bin size, push it there for quick reuse
    if (block_ptr->size == FASTBIN_SIZE) {
        fastbin_push_128(block_ptr);
        total_free_size += (block_ptr->size + sizeof(block_meta_t));
        return;
    }

    insert_free_block(block_ptr);
    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    coalesce_block(block_ptr);
}

void* ff_malloc(size_t size) {
    return bf_malloc(size);
}

void ff_free(void* ptr) {
    bf_free(ptr);
}

