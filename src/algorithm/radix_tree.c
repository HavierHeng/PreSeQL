#include "radix_tree.h"

void radix_insert(RadixNode *root, uint32_t page_num, int32_t slot) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & 0xFF;
        if (!root->children[idx]) {
            root->children[idx] = (RadixNode *)calloc(1, sizeof(RadixNode));
        }
        root = root->children[idx];
    }
    root->status = slot;
}


int32_t radix_lookup(RadixNode *root, uint32_t page_num) {
    for (int level = 0; level < 4; ++level) {
        int idx = (page_num >> ((3 - level) * RADIX_BITS)) & 0xFF;
        if (!root->children[idx]) return -1;
        root = root->children[idx];
    }
    return root->status;
}

// Walk through radix tree and update free status
void walk_radix(RadixNode *node, uint32_t prefix, int depth, void (*cb)(uint32_t page, int slot)) {
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
*/ 

/*
// e.g free callback to make journal page freed to be used in walk()
void free_slot_callback(uint32_t page, int slot) {
    free_list.free_slots[free_list.count++] = slot;
}
*/

