#include "radix_tree.h"


/* TODO:
 * Radix Tree Operations I need to implement
 * 1) Freelist -> Radix Tree conversion - from disk representation of freelist to memory representation as radix tree - free list is capped at some fixed size in the Header
 * 2) Radix Tree -> Free - for saving changes back to ddisk ovcasionally - the number of changes before saving is set by a BATCH_SIZE variable
 * 3) Insert - e.g when page is deleted, i need to add a new page to be tracked
 * 4) Delete - e.g when page is reused, i need to delete it from radix tree
 * 5) Walk - if i cannot find the free value, then I simply do a linear search by walking. Alternatively, since i likely track the highest page being used in my page header and journal header, i can just use this for something else
 * 6) Pop min/find first - Obviously to get the lowest free page available for journal/allocating as data/index/overflow page
 * 7) init and destroy radix tree - for setup and cleanup
 * Extra stuff: There is a struct for RadixTree which contains a pointer to the root RadixNode, that is not implemented
 * Extra stuff 2: Radix Tree is to use the arena_allocator in ../allocator/arena.h - this is just due to the size of the radix tree
*/ 


// Helper function for internal use on Radix Nodes
static void radix_insert(RadixNode *root, uint32_t page_num, int32_t slot) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & 0xFF;
        if (!root->children[idx]) {
            root->children[idx] = (RadixNode *)calloc(1, sizeof(RadixNode));
        }
        root = root->children[idx];
    }
    root->status = slot;
}


// Helper function for internal use on Radix Nodes
static int32_t radix_lookup(RadixNode *root, uint32_t page_num) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & 0xFF;
        if (!root->children[idx]) return -1;
        root = root->children[idx];
    }
    return root->status;
}

// Walk through radix tree and perform callback (e.g update free status)
static void walk_radix(RadixNode *node, uint32_t prefix, int depth, void (*cb)(uint32_t page, int slot)) {
    if (!node) return;

    if (depth == 4) {
        if (node->status != -1) {
            cb(prefix, node->status);
        }
        return;
    }

    for (int i = 0; i < RADIX_SIZE; ++i) {
        if (node->children[i]) {
            walk_radix(node->children[i], (prefix << RADIX_BITS) | i, depth + 1, cb);
        }
    }
}
/*
// e.g free callback to make journal page freed to be used in walk()
void free_slot_callback(uint32_t page, int slot) {
    free_list.free_slots[free_list.count++] = slot;
}
*/

// Public API implementations that match the header declarations
RadixTree* radix_tree_create() {
    RadixTree* tree = (RadixTree*)calloc(1, sizeof(RadixTree));
    // Root node is already zeroed by calloc
    return tree;
}

void radix_tree_destroy(RadixTree* tree) {
    // Helper function to recursively free nodes
    void free_node(RadixNode* node) {
        if (!node) return;
        
        for (int i = 0; i < RADIX_SIZE; i++) {
            if (node->children[i]) {
                free_node(node->children[i]);
                node->children[i] = NULL;
            }
        }
        
        if (node != &tree->root) {
            free(node);
        }
    }
    
    free_node(&tree->root);
    free(tree);
}

void radix_tree_insert(RadixTree *tree, uint32_t page_no, int32_t slot) {
    radix_insert(&tree->root, page_no, slot);
}

void radix_tree_delete(RadixTree *tree, uint32_t page_no) {
    RadixNode* root = &tree->root;
    
    // Navigate to the leaf
    for (int level = 0; level < 4; ++level) {
        int idx = (page_no >> ((3 - level) * RADIX_BITS)) & 0xFF;
        if (!root->children[idx]) return; // Page not found
        root = root->children[idx];
    }
    
    // Mark as not free
    root->status = -1;
}

int32_t radix_tree_pop_min(RadixTree *tree) {
    uint32_t page_num = 0;
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
    
    int32_t slot = root->status;
    if (slot == -1) {
        // This leaf doesn't represent a free page
        return -1;
    }
    
    // Mark as not free
    root->status = -1;
    
    // Return the page number
    return page_num;
}

int32_t radix_tree_lookup(RadixTree *tree, uint32_t page_no) {
    return radix_lookup(&tree->root, page_no);
}

void radix_tree_walk(RadixNode *node, uint32_t prefix, int depth, void (*cb)(uint32_t page, int slot)) {
    walk_radix(node, prefix, depth, cb);
}

// Convert from freelist to radix tree
void freelist_to_radix(RadixTree* tree, uint32_t* freelist, size_t count) {
    for (size_t i = 0; i < count; i++) {
        radix_tree_insert(tree, freelist[i], i); // Using index as slot
    }
}

// Convert from radix tree to freelist
size_t radix_to_freelist(RadixTree* tree, uint32_t* freelist, size_t max_size) {
    size_t count = 0;
    
    void collect_free_pages(uint32_t page, int slot) {
        if (count < max_size) {
            freelist[count++] = page;
        }
    }
    
    radix_tree_walk(&tree->root, 0, 0, collect_free_pages);
    return count;
}

