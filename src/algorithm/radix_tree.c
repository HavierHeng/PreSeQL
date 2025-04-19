// radix_tree.c
#include "radix_tree.h"
#include <stdbool.h>

/* TODO:
 * Radix Tree Operations I need to implement
 * 1) Freelist -> Radix Tree conversion - from disk representation of freelist to memory representation as radix tree - free list is capped at some fixed size in the Header
 * 6) Pop min/find first - Obviously to get the lowest free page available for journal/allocating as data/index/overflow page
 * 7) init and destroy radix tree - for setup and cleanup
 * Extra stuff: There is a struct for RadixTree which contains a pointer to the root RadixNode, that is not implemented
 * Extra stuff 2: Radix Tree is to use the arena_allocator in ../allocator/arena.h - this is just due to the size of the radix tree
*/ 

// TODO: Is this an arena allocator?

// Helper function for internal use on Radix Nodes
// Uses 4 levels with 4 bits each for 16-bit page numbers
// Insert e.g when page is deleted, addition of a new page to be tracked
static void radix_insert(RadixNode *root, uint16_t page_num, int16_t slot) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        if (!root->children[idx]) {
            root->children[idx] = (RadixNode *)calloc(1, sizeof(RadixNode));
        }
        root = root->children[idx];
    }
    root->status = slot;
}

// Helper function for internal use on Radix Nodes
static int16_t radix_lookup(RadixNode *root, uint16_t page_num) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        if (!root->children[idx]) return -1;
        root = root->children[idx];
    }
    return root->status;
}

// Create a tree
RadixTree* radix_tree_create() {
    RadixTree* tree = (RadixTree*)calloc(1, sizeof(RadixTree));
    // Root node is already zeroed by calloc
    return tree;
}


// Helper function to recursively free nodes
void free_node(RadixNode* node, RadixNode* root) {
    if (!node) return;
    
    for (int i = 0; i < RADIX_SIZE; i++) {
        if (node->children[i]) {
            free_node(node->children[i], root);
            node->children[i] = NULL;
        }
    }
    
    // Root node should not be freed from tree struct
    if (node != root) {
        free(node);
    }
}

void radix_tree_destroy(RadixTree* tree) {
    free_node(&tree->root, &tree->root);
    free(tree);
}

void radix_tree_insert(RadixTree *tree, uint16_t page_no, int16_t slot) {
    radix_insert(&tree->root, page_no, slot);
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
    root->status = -1;
}

int16_t radix_tree_pop_min(RadixTree *tree) {
    uint16_t page_num = 0;
    RadixNode* root = &tree->root;
    
    // Navigate to the leftmost leaf with status != -1
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
        if (!found) return -1; // No free pages
    }
    
    int16_t slot = root->status;
    if (slot == -1) {
        // This leaf doesn't represent a free page
        return -1;
    }
    
    // Mark as not free
    root->status = -1;
    
    // Return the page number
    return page_num;
}

int16_t radix_tree_lookup(RadixTree *tree, uint16_t page_no) {
    return radix_lookup(&tree->root, page_no);
}

// Recursive function - Walk through radix tree and perform callback (e.g update free status, and building freelist to serialization to disk)
void walk_radix(RadixNode *node, uint16_t prefix, int depth, void (*cb)(uint16_t page, int16_t slot, void* user_data), void* user_data) {
    if (!node) return;

    // 16 bits with stride of 4 gives 4 levels - Base case leaf node
    if (depth == 4) {
        if (node->status != -1) {
            cb(prefix, node->status, user_data);
        }
        return;
    }

    // Recurse: Non-leave node - call on children
    for (int i = 0; i < RADIX_SIZE; ++i) {
        if (node->children[i]) {
            walk_radix(node->children[i], (prefix << RADIX_BITS) | i, depth + 1, cb, user_data);
        }
    }
}

// Start walking through the tree and applying callback function
// Callback function operates on a page no. a slot and some user data that can be used to pass in some data
void radix_tree_walk(RadixTree *tree, void (*cb)(uint16_t page, int16_t slot, void* user_data), void* user_data) {
    walk_radix(&tree->root, 0, 0, cb, user_data);
}

static void collect_free_pages(uint16_t page, int16_t slot, uint16_t* freelist, size_t* count, size_t max_size) {
    if (*count < max_size) {
        freelist[(*count)++] = page;
    }
}

// Adapter function to match callback signature
static void collect_adapter(uint16_t page, int16_t slot, void* user_data) {
    struct {
        uint16_t* freelist;
        size_t* count;
        size_t max_size;
    }* data = user_data;
    
    collect_free_pages(page, slot, data->freelist, data->count, data->max_size);
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
    
    radix_tree_walk(tree, collect_adapter, &data);
    return count;
}


// Convert from freelist to radix tree
void freelist_to_radix(RadixTree* tree, uint16_t* freelist, size_t count) {
    for (size_t i = 0; i < count; i++) {
        radix_tree_insert(tree, freelist[i], i); // Using index as slot
    }
}

