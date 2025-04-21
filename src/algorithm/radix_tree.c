// radix_tree.c
#define ARENA_IMPLEMENTATION
#include "radix_tree.h"

// Helper function for internal use on Radix Nodes
// Uses 4 levels with 4 bits each for 16-bit page numbers (both db and journal have max page of 65535)
// Insert e.g when page is deleted, addition of a new page to be tracked


// Create a tree using Arena
RadixTree* radix_tree_create() {
    RadixTree* tree = (RadixTree*)malloc(sizeof(RadixTree));
    if (!tree) return NULL;  // failled to malloc tree

    memset(tree, 0, sizeof(RadixTree)); // Zero-initialize arena memory (just in case it has stuff)
    tree->arena.begin = NULL;
    tree->arena.end = NULL; // Initialize empty arena
    return tree;
}

// Destroy tree using Arena 
// Easy non-recursive way by dumping the whole tree's arena
void radix_tree_destroy(RadixTree *tree) {
    if (!tree) return;
    arena_free(&tree->arena); // Free all arena-allocated memory (all nodes)
    free(tree); // Free the tree struct
}

void radix_tree_insert(RadixTree *tree, uint16_t page_no) {
    // if (page_no == 0) return;  // Page 0 is the base metadata - but just in case this introduces bugs, imma comment it out

    RadixNode *current = &tree->root;  // Start from root node

    // Create path to represent this page number
    for (int level = 0; level < 4; level++) {
        int idx = (page_no >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        
        if (!current->children[idx]) {
            current->children[idx] = arena_alloc(&tree->arena, sizeof(RadixNode));
            memset(current->children[idx], 0, sizeof(RadixNode));
        }
        
        current = current->children[idx];
    }

}

bool radix_tree_lookup(RadixTree *tree, uint16_t page_no) {
    if (page_no == 0) return false;
    
    RadixNode *current = &tree->root;
    
    // Follow the path for this page number - check each level and find the idx to jump to next
    for (int level = 0; level < 4; level++) {
        int idx = (page_no >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        
        if (!current->children[idx]) {
            return false; // Path doesn't exist, page is not free
        }
        
        current = current->children[idx];
    }
    
    return true; // Complete path exists, page is free
}

// Helper function check if node has children
static bool has_children(RadixNode *node) {
    for (int i = 0; i < RADIX_SIZE; i++) {
        if (node->children[i]) return true;
    }
    return false;
}

// Delete e.g when page is reused, deletion of entry from radix tree
// Idea is to find path to the page, then once leaf found,
// Go back up the radix tree, updating/cleaning any parents with no children left after this deletion operation
void radix_tree_delete(RadixTree *tree, uint16_t page_no) {
    if (page_no == 0 || !tree) return;
    
    RadixNode* path[4] = {NULL};
    int indices[4] = {0};
    
    RadixNode *current = &tree->root;
    
    // Find the path to the page
    for (int level = 0; level < 4; level++) {
        int idx = (page_no >> ((3 - level) * RADIX_BITS)) & (RADIX_SIZE - 1);
        
        if (!current->children[idx]) {
            return; // Page not in tree (not free)
        }
        
        path[level] = current;
        indices[level] = idx;
        current = current->children[idx];
    }
    
    // Remove leaf node and clean up any empty parents
    // Start from the leaf and work backwards
    for (int level = 3; level >= 0; level--) {
        RadixNode *parent = path[level];
        int idx = indices[level];
        
        if (level == 3 || !has_children(parent->children[idx])) {
            // This node has no children, safe to remove
            parent->children[idx] = NULL;
        } else {
            // This node has children, stop the cleanup
            break;
        }
    }
}

// Get the smallest page number in the tree without removing it
uint16_t radix_tree_peek_min(RadixTree *tree) {
    if (!tree) return 0;
    
    uint16_t page_num = 0;
    RadixNode *current = &tree->root;
    
    // Find the leftmost path (smallest page number)
    for (int level = 0; level < 4; level++) {
        int idx = 0;
        while (idx < RADIX_SIZE && !current->children[idx]) {
            idx++;
        }
        
        if (idx == RADIX_SIZE) {
            return 0; // No pages in tree
        }
        
        page_num = (page_num << RADIX_BITS) | idx;
        current = current->children[idx];
    }
    
    return page_num;
}

// Get and remove the smallest page number in the tree
// Get the minimum entry at the head of the tree
uint16_t radix_tree_pop_min(RadixTree *tree) {
    if (!tree) return 0;
    
    uint16_t page_num = 0;
    RadixNode *path[4] = {NULL};
    int indices[4] = {0};
    
    RadixNode *current = &tree->root;
    path[0] = current;
    
    // Find the leftmost path (smallest page number)
    for (int level = 0; level < 4; level++) {
        int idx = 0;
        while (idx < RADIX_SIZE && !current->children[idx]) {
            idx++;
        }
        
        if (idx == RADIX_SIZE) {
            return 0; // No pages in tree
        }
        
        indices[level] = idx;
        page_num = (page_num << RADIX_BITS) | idx;
        
        if (level < 3) {
            path[level+1] = current->children[idx];
            current = current->children[idx];
        }
    }
    
    // Delete the path for this page
    radix_tree_delete(tree, page_num);
    
    return page_num;
}

// Recursive function - Walk through radix tree and perform callback (e.g update free status, and building freelist to serialization to disk)
static void walk_radix(RadixNode *node, uint16_t prefix, int depth, 
                      void (*cb)(uint16_t page, void* user_data), void* user_data) {
    if (!node) return;
    
    // If at leaf level, this is a free page
    if (depth == 4) {
        cb(prefix, user_data);
        return;
    }
    
    // Recurse through all children
    for (int i = 0; i < RADIX_SIZE; i++) {
        if (node->children[i]) {
            uint16_t new_prefix = (prefix << RADIX_BITS) | i;
            walk_radix(node->children[i], new_prefix, depth + 1, cb, user_data);
        }
    }
}

// Start walking through the tree and applying callback function
// Callback function operates on a page no. a slot and user data that can be used to pass data from within the callback out to the calling function
void radix_tree_walk(RadixTree *tree, void (*cb)(uint16_t page, void* user_data), void* user_data) {
    if (!tree) return;
    walk_radix(&tree->root, 0, 0, cb, user_data);
}

// Helper function for radix_to_freelist - takes freed pages stored in Radix Tree and puts it into a user_data field for us to extract later
// We need this for serialization to disk header
static void collect_pages(uint16_t page, void* user_data) {
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
    if (!tree || !freelist) return 0;
    
    size_t count = 0;
    
    struct {
        uint16_t* freelist;
        size_t* count;
        size_t max_size;
    } data = {freelist, &count, max_size};
    
    radix_tree_walk(tree, collect_pages, &data);
    return count;
}


// Convert from freelist to radix tree
void freelist_to_radix(RadixTree* tree, uint16_t* freelist, size_t count) {
    if (!tree || !freelist) return;
    
    for (size_t i = 0; i < count; i++) {
        if (freelist[i] != 0) {
            radix_tree_insert(tree, freelist[i]);
        }
    }
}
