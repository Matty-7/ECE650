#include "my_malloc.h"
#include <unistd.h>     // for sbrk
#include <stddef.h>     // for NULL, size_t
#include <stdint.h>     // for intptr_t


typedef struct tree_node {
    struct tree_node* left;
    struct tree_node* right;
    int height;
    size_t size;            // the same as block->size, used as the key for AVL tree
    block_meta_t* block;    // pointer to the actual free block
} tree_node_t;

// root of the free tree (AVL)
static tree_node_t* free_root = NULL;

// fastbin for blocks of size == 128 (single linked list)
static block_meta_t* fastbin_128 = NULL;

static unsigned long total_size = 0;      // heap size including metadata
static unsigned long total_free_size = 0; // current free blocks (including metadata)

static const size_t FASTBIN_SIZE = 128;   // use fastbin when size == 128


static size_t align8(size_t size);

static block_meta_t* request_space(size_t size);
static void split_block(block_meta_t* block, size_t size);
static void coalesce_block(block_meta_t* block);

static void insert_free_block(block_meta_t* block);
static void remove_free_block(block_meta_t* block);

static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block);
static tree_node_t* avl_delete(tree_node_t* node, size_t size, block_meta_t* block);

static int height(tree_node_t* node);
static void update_height(tree_node_t* node);
static int get_balance(tree_node_t* node);
static tree_node_t* left_rotate(tree_node_t* x);
static tree_node_t* right_rotate(tree_node_t* y);

static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size);
static tree_node_t* find_first_fit_node(tree_node_t* root, size_t size);

static block_meta_t* find_adjacent_block(tree_node_t* root, block_meta_t* block);

static block_meta_t* get_block_ptr(void* ptr);


unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

/**
 * Best Fit malloc
 */
void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = align8(size);

    // try to get from fastbin_128 when size == 128
    if (size == FASTBIN_SIZE) {
        if (fastbin_128) {
            block_meta_t* fast_block = fastbin_128;
            fastbin_128 = fast_block->next;   // pop
            fast_block->next = NULL;
            fast_block->free = 0;
            total_free_size -= (fast_block->size + sizeof(block_meta_t));
            return (char*)fast_block + sizeof(block_meta_t);
        }
    }

    // otherwise, find the best fit block in the AVL tree
    tree_node_t* best_node = find_best_fit_node(free_root, size);
    block_meta_t* block = NULL;
    if (!best_node) {
        // no available block, request from the top of the heap
        block = request_space(size);
        if (!block) {
            return NULL;
        }
    } else {
        // found a suitable block
        block = best_node->block;
        remove_free_block(block); // remove from the free tree
        block->free = 0;
        total_free_size -= (block->size + sizeof(block_meta_t));
        // split
        split_block(block, size);
    }

    return (char*)block + sizeof(block_meta_t);
}

/**
 * Best Fit free
 */
void bf_free(void* ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block = get_block_ptr(ptr);
    if (block->free) {
        // ignore double-free
        return;
    }
    block->free = 1;

    // if size == 128, put into fastbin
    if (block->size == FASTBIN_SIZE) {
        block->next = fastbin_128;
        fastbin_128 = block;
        total_free_size += (block->size + sizeof(block_meta_t));
        return;
    }

    // otherwise, insert back into AVL
    insert_free_block(block);
    total_free_size += (block->size + sizeof(block_meta_t));

    // coalesce adjacent free blocks
    coalesce_block(block);
}

/**
 * First Fit malloc
 */
void* ff_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = align8(size);

    // try to use fastbin when size == 128
    if (size == FASTBIN_SIZE) {
        if (fastbin_128) {
            block_meta_t* fast_block = fastbin_128;
            fastbin_128 = fast_block->next;
            fast_block->next = NULL;
            fast_block->free = 0;
            total_free_size -= (fast_block->size + sizeof(block_meta_t));
            return (char*)fast_block + sizeof(block_meta_t);
        }
    }

    // find the first free block that can hold size in the AVL tree
    tree_node_t* first_node = find_first_fit_node(free_root, size);
    block_meta_t* block = NULL;
    if (!first_node) {
        // no available block, request from the top of the heap
        block = request_space(size);
        if (!block) {
            return NULL;
        }
    } else {
        block = first_node->block;
        remove_free_block(block);
        block->free = 0;
        total_free_size -= (block->size + sizeof(block_meta_t));
        split_block(block, size);
    }

    return (char*)block + sizeof(block_meta_t);
}

/**
 * First Fit free
 */
void ff_free(void* ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block = get_block_ptr(ptr);
    if (block->free) {
        return;  // ignore duplicate free
    }
    block->free = 1;

    if (block->size == FASTBIN_SIZE) {
        block->next = fastbin_128;
        fastbin_128 = block;
        total_free_size += (block->size + sizeof(block_meta_t));
        return;
    }

    insert_free_block(block);
    total_free_size += (block->size + sizeof(block_meta_t));

    coalesce_block(block);
}

/** 8字节对齐 */
static size_t align8(size_t size) {
    return (size + 7) & ~((size_t)7);
}

/** get the start address of block_meta_t   from user pointer */
static block_meta_t* get_block_ptr(void* ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}

/** request new heap space (if no suitable free block in AVL) */
static block_meta_t* request_space(size_t size) {
    block_meta_t* block = sbrk(0);
    void* request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*)-1) {
        // sbrk失败
        return NULL;
    }
    block->size = size;
    block->free = 0;
    block->prev = NULL;
    block->next = NULL;

    total_size += (size + sizeof(block_meta_t));
    return block;
}

/**
 * if the free block is much larger than needed, split it into two
 * the new split-off "remaining block" is inserted into AVL
 */
static void split_block(block_meta_t* block, size_t size) {
    // ensure there is enough space for a meta + some user space
    if (block->size >= size + sizeof(block_meta_t) + 8) {
        char* address = (char*)block;
        block_meta_t* new_block = (block_meta_t*)(address + sizeof(block_meta_t) + size);

        new_block->size = block->size - size - sizeof(block_meta_t);
        new_block->free = 1;
        new_block->prev = NULL;
        new_block->next = NULL;

        block->size = size;

        // insert the new block as free
        insert_free_block(new_block);
        total_free_size += (new_block->size + sizeof(block_meta_t));
    }
}

/**
 * coalesce the current block with its adjacent free block (may need to repeat)
 */
static void coalesce_block(block_meta_t* block) {
    while (1) {
        // find the free block adjacent to it by physical address
        block_meta_t* buddy = find_adjacent_block(free_root, block);
        if (!buddy) {
            break;  // no adjacent free block
        }
        // remove both from AVL
        remove_free_block(buddy);
        remove_free_block(block);

        // determine which one is left and which is right
        block_meta_t* left = (block < buddy) ? block : buddy;
        block_meta_t* right = (left == block) ? buddy : block;

        // merge into left
        left->size += right->size + sizeof(block_meta_t);
        // this meta is also "swallowed", so free_size is increased by one meta
        total_free_size += sizeof(block_meta_t);

        // insert the merged large block back into AVL
        insert_free_block(left);

        // update block, continue loop to see if it can be merged with other blocks
        block = left;
    }
}


static void insert_free_block(block_meta_t* block) {
    free_root = avl_insert(free_root, block);
}

static void remove_free_block(block_meta_t* block) {
    free_root = avl_delete(free_root, block->size, block);
}

static int height(tree_node_t* node) {
    return node ? node->height : 0;
}

static void update_height(tree_node_t* node) {
    int hl = height(node->left);
    int hr = height(node->right);
    node->height = (hl > hr ? hl : hr) + 1;
}

static int get_balance(tree_node_t* node) {
    if (!node) return 0;
    return height(node->left) - height(node->right);
}

static tree_node_t* right_rotate(tree_node_t* y) {
    tree_node_t* x = y->left;
    tree_node_t* T2 = x->right;

    // rotate
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

/** insert block into AVL, key = block->size */
static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block) {
    if (!node) {
        // allocate space for tree node directly using sbrk (may also increase total_size, consider it as management overhead)
        tree_node_t* new_node = (tree_node_t*)sbrk(sizeof(tree_node_t));
        if (new_node == (void*)-1) {
            return NULL; // sbrk failed
        }
        new_node->left = new_node->right = NULL;
        new_node->height = 1;
        new_node->size = block->size;
        new_node->block = block;
        // if needed, can: total_size += sizeof(tree_node_t);
        return new_node;
    }

    if (block->size < node->size) {
        node->left = avl_insert(node->left, block);
    } else {
        node->right = avl_insert(node->right, block);
    }

    update_height(node);

    int balance = get_balance(node);

    // classic AVL four rotations
    if (balance > 1 && block->size < node->left->size) {
        return right_rotate(node);
    }
    if (balance < -1 && block->size >= node->right->size) {
        return left_rotate(node);
    }
    if (balance > 1 && block->size >= node->left->size) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
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

/** delete a node with size = size and pointer = block from AVL */
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
        // check if it is the same block
        if (node->block == block) {
            // find the node to delete
            if (!node->left && !node->right) {
                node = NULL;
            } else if (!node->left) {
                node = node->right;
            } else if (!node->right) {
                node = node->left;
            } else {
                // both left and right subtrees exist
                tree_node_t* temp = min_value_node(node->right);
                node->size = temp->size;
                node->block = temp->block;
                node->right = avl_delete(node->right, temp->size, temp->block);
            }
            return node;
        } else {
            // size is the same, but not the same block pointer, search in right subtree
            node->right = avl_delete(node->right, size, block);
        }
    }

    if (!node) {
        return NULL;
    }

    // update height & rebalance
    update_height(node);
    int balance = get_balance(node);

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


/** Best Fit: find the smallest block that is >= size in AVL */
static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size) {
    tree_node_t* best = NULL;
    tree_node_t* current = root;
    while (current) {
        if (current->size == size) {
            // exactly match
            best = current;
            break;
        } else if (current->size > size) {
            // record this node, then continue searching left subtree for smaller but still >= size
            best = current;
            current = current->left;
        } else {
            // current->size < size
            current = current->right;
        }
    }
    return best;
}

/** First Fit: return immediately when >= size is found */
static tree_node_t* find_first_fit_node(tree_node_t* root, size_t size) {
    if (!root) return NULL;
    if (root->size >= size) {
        return root; // return immediately
    }
    // otherwise, continue searching left and right
    tree_node_t* left_res = find_first_fit_node(root->left, size);
    if (left_res) return left_res;
    return find_first_fit_node(root->right, size);
}

/** helper: search in AVL for free block adjacent to block (physical address contiguous) */
static block_meta_t* find_adjacent_block(tree_node_t* root, block_meta_t* block) {
    if (!root) {
        return NULL;
    }
    block_meta_t* candidate = root->block;
    // physical address
    char* caddr = (char*)candidate;
    char* baddr = (char*)block;

    // candidate is immediately adjacent to block on the right
    if (caddr == baddr + sizeof(block_meta_t) + block->size) {
        return candidate;
    }
    // candidate is immediately adjacent to block on the left
    if (baddr == caddr + sizeof(block_meta_t) + candidate->size) {
        return candidate;
    }
    // continue searching left and right subtrees
    block_meta_t* left_res = find_adjacent_block(root->left, block);
    if (left_res) return left_res;
    return find_adjacent_block(root->right, block);
}
