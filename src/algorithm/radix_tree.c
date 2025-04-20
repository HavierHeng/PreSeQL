// radix_tree.c
#include "radix_tree.h"

// Helper function for internal use on Radix Nodes
// Uses 4 levels with 4 bits each for 16-bit page numbers (both db and journal have max page of 65535)
// Insert e.g when page is deleted, addition of a new page to be tracked
static void radix_insert(RadixNode *root, uint16_t page_num, Arena *arena) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        if (!root->children[idx]) {
            root->children[idx] = (RadixNode *)arena_alloc(arena, sizeof(RadixNode));
            memset(root->children[idx], 0, sizeof(RadixNode)); // Zero-initialize
        }
        root = root->children[idx];
    }
    root->is_free = 1; // Mark as free
}

// Helper function for internal use on Radix Nodes
static bool radix_lookup(RadixNode *root, uint16_t page_num) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        if (!root->children[idx]) return false;
        root = root->children[idx];
    }
    return root->is_free; // Return free status
}


// Create a tree using Arena
RadixTree* radix_tree_create() {
    RadixTree* tree = (RadixTree*)malloc(sizeof(RadixTree));
    memset(tree, 0, sizeof(RadixTree)); // Zero-initialize
    tree->arena.begin = NULL;
    tree->arena.end = NULL; // Initialize empty arena
    return tree;
}

// Destroy tree using Arena - Easy non-recursive way by dumping the whole tree
void radix_tree_destroy(RadixTree* tree) {
    arena_free(&tree->arena); // Free all arena-allocated memory (all nodes)
    free(tree); // Free the tree struct
}

// Insert a page
void radix_tree_insert(RadixTree *tree, uint16_t page_no) {
    radix_insert(&tree->root, page_no, &tree->arena);
}

// Delete e.g when page is reused, deletion of entry from radix tree
void radix_tree_delete(RadixTree *tree, uint16_t page_no) {
    RadixNode* root = &tree->root;
    
    // Navigate to the leaf
    for (int level = 0; level < 4; ++level) {
        int idx = (page_no >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        if (!root->children[idx]) return; // Page not found
        root = root->children[idx];
    }
    
    // Mark as not free
    root->is_free = 0;
}

uint16_t radix_tree_pop_min(RadixTree *tree) {
    uint16_t page_num = 0;
    RadixNode* root = &tree->root;
    
    // Navigate to the leftmost leaf with is_free = 1
    for (int level = 0; level < 4; ++level) {
        bool found = false;
        for (int i = 0; i < RADIX_SIZE; i++) {
            if (root->children[i]) {
                page_num = (page_num << RADIX_BITS) | i;
                root = root->children[i];
                found = true;
                break;
            }
        }
        if (!found) return 0; // No free pages (0 is invalid page_no)
    }
    
    if (!root->is_free) {
        // This leaf is not free
        return 0;
    }
    
    // Mark as not free
    root->is_free = 0;
    
    // Return the page number
    return page_num;
}

bool radix_tree_lookup(RadixTree *tree, uint16_t page_no) {
    return radix_lookup(&tree->root, page_no);
}

// Recursive function - Walk through radix tree and perform callback (e.g update free status, and building freelist to serialization to disk)
void walk_radix(RadixNode *node, uint16_t prefix, int depth, void (*cb)(uint16_t page, void* user_data), void* user_data) {
    if (!node) return;

    // 16 bits with stride of 4 gives 4 levels - Base case leaf node
    if (depth == 4) {
        if (node->is_free) {
            cb(prefix, user_data);
        }
        return;
    }

    // Recurse: Non-leaf node - call on children
    for (int i = 0; i < RADIX_SIZE; ++i) {
        if (node->children[i]) {
            walk_radix(node->children[i], (prefix << RADIX_BITS) | i, depth + 1, cb, user_data);
        }
    }
}
// Start walking through the tree and applying callback function
// Callback function operates on a page no. a slot and some user data that can be used to pass in some data
void radix_tree_walk(RadixTree *tree, void (*cb)(uint16_t page, void* user_data), void* user_data) {
    walk_radix(&tree->root, 0, 0, cb, user_data);
}

static void collect_free_pages(uint16_t page, void* user_data) {
    struct {
        uint16_t* freelist;
        size_t* count;
        size_t max_size;
    }* data = user_data;
    
    if (*data->count < data->max_size) {
        data->freelist[(*data->count)++] = page;
    }
}

// Convert from radix tree to freelist
// This is for serializing back to the Header on disk
size_t radix_to_freelist(RadixTree* tree, uint16_t* freelist, size_t max_size) {
    size_t count = 0;

    // Pass in struct of output buffers for the callback to fill up
    // e.g location of freelist (which is a disk location) to be modified
    struct {
        uint16_t* freelist;
        size_t* count;
        size_t max_size;
    } data = {freelist, &count, max_size};
    
    radix_tree_walk(tree, collect_free_pages, &data);
    return count;
}


// Convert from freelist to radix tree
void freelist_to_radix(RadixTree* tree, uint16_t* freelist, size_t count) {
    for (size_t i = 0; i < count; i++) {
        radix_tree_insert(tree, freelist[i]);
    }
}
