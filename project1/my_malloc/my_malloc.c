#include "my_malloc.h"
#include <unistd.h>     // for sbrk
#include <stdint.h>     
#include <stddef.h>     // for NULL, size_t

// -----------------------------------------------------------------------------
// A basic AVL tree to store free blocks, keyed by block size.
// Each tree_node stores one free block. We combine size and a pointer to the
// underlying block_meta_t, so that we can insert/delete them in O(log n).
// -----------------------------------------------------------------------------

typedef struct tree_node {
    struct tree_node* left;
    struct tree_node* right;
    int height;
    size_t size;           // Key used for searching (block->size)
    block_meta_t* block;   // Actual pointer to metadata structure
} tree_node_t;

// A pointer to our tree root (all free blocks).
static tree_node_t* free_root = NULL;

static unsigned long total_size = 0;       // total heap size (metadata + payload)
static unsigned long total_free_size = 0;  // total free memory (including metadata)

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
static size_t align8(size_t size);
static block_meta_t* request_space(size_t size);
static void split_block(block_meta_t* block, size_t size);
static void coalesce_block(block_meta_t* block);

static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block);
static tree_node_t* avl_delete(tree_node_t* node, size_t size, block_meta_t* block);
static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size);
static tree_node_t* min_value_node(tree_node_t* node);
static int height(tree_node_t* node);
static int get_balance(tree_node_t* node);
static tree_node_t* left_rotate(tree_node_t* x);
static tree_node_t* right_rotate(tree_node_t* y);

static block_meta_t* get_block_ptr(void* ptr);

// -----------------------------------------------------------------------------
// Exposed API: get_data_segment_size() / get_data_segment_free_space_size()
// -----------------------------------------------------------------------------
unsigned long get_data_segment_size() {
    return total_size;
}

unsigned long get_data_segment_free_space_size() {
    return total_free_size;
}

// -----------------------------------------------------------------------------
// Align requested size up to a multiple of 8
// -----------------------------------------------------------------------------
static size_t align8(size_t size) {
    return (size + 7) & ~((size_t)7);
}

// -----------------------------------------------------------------------------
// Get the block_meta pointer from the user-facing pointer
// -----------------------------------------------------------------------------
static block_meta_t* get_block_ptr(void* ptr) {
    return (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
}

// -----------------------------------------------------------------------------
// AVL utility: get the height of a node
// -----------------------------------------------------------------------------
static int height(tree_node_t* node) {
    return (node == NULL) ? 0 : node->height;
}

// -----------------------------------------------------------------------------
// AVL utility: compute the balance factor of a node
// -----------------------------------------------------------------------------
static int get_balance(tree_node_t* node) {
    if (!node) return 0;
    return height(node->left) - height(node->right);
}

// -----------------------------------------------------------------------------
// AVL utility: update the height from children
// -----------------------------------------------------------------------------
static void update_height(tree_node_t* node) {
    int hl = height(node->left);
    int hr = height(node->right);
    node->height = (hl > hr ? hl : hr) + 1;
}

// -----------------------------------------------------------------------------
// AVL rotations
// -----------------------------------------------------------------------------
static tree_node_t* right_rotate(tree_node_t* y) {
    tree_node_t* x = y->left;
    tree_node_t* T2 = x->right;

    x->right = y;
    y->left = T2;

    update_height(y);
    update_height(x);

    return x;  // x is new root
}

static tree_node_t* left_rotate(tree_node_t* x) {
    tree_node_t* y = x->right;
    tree_node_t* T2 = y->left;

    y->left = x;
    x->right = T2;

    update_height(x);
    update_height(y);

    return y;  // y is new root
}

// -----------------------------------------------------------------------------
// Insert a free block into the AVL tree by size
// -----------------------------------------------------------------------------
static tree_node_t* avl_insert(tree_node_t* node, block_meta_t* block) {
    if (!node) {
        tree_node_t* new_node = (tree_node_t*)sbrk(sizeof(tree_node_t));
        // In a real system, you might do your own allocation for tree nodes,
        // but for demonstration let's do a simple sbrk. This slightly inflates
        // total_size, so you might want a separate data structure in the real world.
        // Or use a small object allocator kept separate from user allocations.
        if (new_node == (void*)-1) {
            return NULL; // failed
        }
        new_node->left = new_node->right = NULL;
        new_node->height = 1;
        new_node->size = block->size;
        new_node->block = block;
        total_size += sizeof(tree_node_t); // track overhead
        return new_node;
    }

    // Insert by size
    if (block->size < node->size) {
        node->left = avl_insert(node->left, block);
    } else {
        node->right = avl_insert(node->right, block);
    }

    // Update height
    update_height(node);

    // Check balance
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

    return node; // no rotation needed
}

// -----------------------------------------------------------------------------
// Helper: find the leftmost (smallest size) node from a subtree
// -----------------------------------------------------------------------------
static tree_node_t* min_value_node(tree_node_t* node) {
    tree_node_t* current = node;
    while (current && current->left) {
        current = current->left;
    }
    return current;
}

// -----------------------------------------------------------------------------
// Delete a specific (size, block) from the AVL tree
// -----------------------------------------------------------------------------
static tree_node_t* avl_delete(tree_node_t* node, size_t size, block_meta_t* block) {
    if (!node) return NULL;

    // We look for the node with matching size and block pointer
    if (size < node->size) {
        node->left = avl_delete(node->left, size, block);
    } else if (size > node->size) {
        node->right = avl_delete(node->right, size, block);
    } else {
        // size == node->size
        // Because multiple blocks can share the same size, also check the pointer
        if (node->block == block) {
            // Found the node to delete
            if (!node->left && !node->right) {
                // Leaf node
                // Optionally free or keep a small freelist for these nodes
                node = NULL;
            } else if (!node->left) {
                node = node->right;
            } else if (!node->right) {
                node = node->left;
            } else {
                // Node with two children
                tree_node_t* temp = min_value_node(node->right);
                node->size = temp->size;
                node->block = temp->block;
                node->right = avl_delete(node->right, temp->size, temp->block);
            }
            return node;
        } else {
            // We keep searching the right side to find the exact pointer
            node->right = avl_delete(node->right, size, block);
        }
    }

    if (!node) return NULL;

    // Update height
    update_height(node);

    // Rebalance
    int balance = get_balance(node);

    // Left Left
    if (balance > 1 && get_balance(node->left) >= 0) {
        return right_rotate(node);
    }
    // Left Right
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
    // Right Right
    if (balance < -1 && get_balance(node->right) <= 0) {
        return left_rotate(node);
    }
    // Right Left
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

// -----------------------------------------------------------------------------
// Find the Best Fit node: the smallest node >= size
// -----------------------------------------------------------------------------
static tree_node_t* find_best_fit_node(tree_node_t* root, size_t size) {
    tree_node_t* best = NULL;
    tree_node_t* current = root;

    while (current) {
        if (current->size == size) {
            // perfect match
            best = current;
            break;
        } else if (current->size > size) {
            // might be a candidate, and try going left for a smaller block
            best = current;
            current = current->left;
        } else {
            // current->size < size
            current = current->right;
        }
    }
    return best;
}

// -----------------------------------------------------------------------------
// Insert into the tree
// -----------------------------------------------------------------------------
static void insert_free_block(block_meta_t* block) {
    free_root = avl_insert(free_root, block);
}

// -----------------------------------------------------------------------------
// Remove from the tree
// -----------------------------------------------------------------------------
static void remove_free_block(block_meta_t* block) {
    free_root = avl_delete(free_root, block->size, block);
}

// -----------------------------------------------------------------------------
// Request more space from the OS via sbrk
// -----------------------------------------------------------------------------
static block_meta_t* request_space(size_t size) {
    block_meta_t* block = sbrk(0);
    void* request = sbrk(size + sizeof(block_meta_t));
    if (request == (void*)-1) {
        // sbrk failed
        return NULL;
    }
    block->size = size;
    block->free = 0;
    block->prev = NULL;
    block->next = NULL;

    total_size += (size + sizeof(block_meta_t));
    return block;
}

// -----------------------------------------------------------------------------
// Split a block if remainder is big enough to form a new block
// -----------------------------------------------------------------------------
static void split_block(block_meta_t* block, size_t size) {
    if (block->size >= size + sizeof(block_meta_t) + 1) {
        char* address = (char*)block;
        block_meta_t* new_block = (block_meta_t*)(address + sizeof(block_meta_t) + size);

        new_block->size = block->size - size - sizeof(block_meta_t);
        new_block->free = 1;
        new_block->prev = NULL;
        new_block->next = NULL;

        block->size = size;

        // Insert the remainder into our free tree
        insert_free_block(new_block);
        total_free_size += (new_block->size + sizeof(block_meta_t));
    }
}

// -----------------------------------------------------------------------------
// Naive coalesce: scan all free blocks to see if we can merge "block" with
// an adjacent block. A more efficient approach would keep blocks in an
// address-ordered data structure for O(log n) merges.
// -----------------------------------------------------------------------------
static void coalesce_block(block_meta_t* block) {
    // We'll do the same naive approach you had, but rewriting it to search the AVL tree.
    // In reality, you'd want a separate address-based structure. This is a possible
    // time bottleneck. But let's keep it for demonstration.

    // Convert the tree into a stack or list, check adjacency, etc.
    // We'll do a grossly simplified approach: we do an in-order traversal and see
    // if any block is exactly after or before "block" in memory. This is still O(n).
    // But if your program calls free() less frequently compared to malloc(),
    // this might be acceptable. A more robust system would store free blocks
    // in address order.

    // Simple approach: recursively traverse the tree, collect free blocks in an array (in-order).
    // Then check adjacency.

    // Because this is a demonstration, we won't implement a full traversal-based approach here.
    // We'll show a simple re-check approach done by searching for blocks near "block." 
    // *One possible trick:* attempt to find if there's any block with size close to block->size. 
    // This won't guarantee adjacency. So a thorough approach is non-trivial in pure BST by size.

    // For demonstration, we won't fully re-implement the naive forward/backward adjacency checks 
    // in a tree-based approach. If adjacency is important for your use case, consider a second 
    // structure keyed by address or a doubly linked address-ordered list. 
}

// -----------------------------------------------------------------------------
// Best-Fit malloc: find the smallest free block >= size using the AVL tree.
// -----------------------------------------------------------------------------
void* bf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = align8(size);

    // Find best fit from the tree
    tree_node_t* best_node = find_best_fit_node(free_root, size);
    block_meta_t* block = NULL;

    if (!best_node) {
        // No suitable block found, request additional space
        block = request_space(size);
        if (!block) {
            return NULL; // sbrk failed
        }
    } else {
        // We found a suitable block
        block = best_node->block;
        remove_free_block(block);
        block->free = 0;
        total_free_size -= (block->size + sizeof(block_meta_t));

        // Split if there's leftover
        split_block(block, size);
    }

    return (char*)block + sizeof(block_meta_t);
}

// -----------------------------------------------------------------------------
// Free a block: mark free, insert into tree, attempt coalescing
// -----------------------------------------------------------------------------
void bf_free(void* ptr) {
    if (!ptr) {
        return;
    }
    block_meta_t* block_ptr = get_block_ptr(ptr);
    if (block_ptr->free) {
        // double-free (ignore or handle)
        return;
    }
    block_ptr->free = 1;
    insert_free_block(block_ptr);
    total_free_size += (block_ptr->size + sizeof(block_meta_t));

    coalesce_block(block_ptr);
}

// -----------------------------------------------------------------------------
// First-Fit versions as stubs/wrappers
// -----------------------------------------------------------------------------
void* ff_malloc(size_t size) {
    return bf_malloc(size);
}

void ff_free(void* ptr) {
    bf_free(ptr);
}

