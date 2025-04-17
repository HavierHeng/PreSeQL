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

/*
// e.g free callback to make journal page freed
void free_slot_callback(uint32_t page, int slot) {
    free_list.free_slots[free_list.count++] = slot;
}
*/

