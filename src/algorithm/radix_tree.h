#ifndef RADIX_ALGORITHM_H
#define RADIX_ALGORITHM_H
// Radix tree is for finding and freeing free pages in O(log n) time
// This is used in a few parts of the code: 
// 1) Finding Free pages in Pager - For Reuse of pages, to prevent fragmentation
// 2) Free Pages in Journal if Transaction is used - For Reuse, such as when pages are dirty, we make a copy
// This is as pages in the database can get huge in number, and is dynamic in count. 
// This job to monitor freed pages cannot be handled by just a bitmap alone, while a free list is inefficient.

// There are more complex implementations of a radix tree
// e.g storing values different from a sorting key 
// but in my case, the mere presence of an item in the RadixTree mean that it fulfils some condition 
// e.g is a freed page, and once it is reused its removed
// e.g if it has a size of mostly_full, it is added to the radix tree bucket 

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "allocator/arena.h"

#define RADIX_BITS 4
#define RADIX_SIZE (1 << RADIX_BITS)

typedef struct RadixNode {
    struct RadixNode *children[RADIX_SIZE];  // next level
} RadixNode;

typedef struct RadixTree {
    RadixNode root;
    Arena arena; // Per-tree arena for memory management
} RadixTree;

RadixTree* radix_tree_create();
void radix_tree_destroy(RadixTree* tree);

// Basic Radix Tree operations
void radix_tree_insert(RadixTree *tree, uint16_t page_no);
void radix_tree_delete(RadixTree *tree, uint16_t page_no);
uint16_t radix_tree_peek_min(RadixTree *tree);
uint16_t radix_tree_pop_min(RadixTree *tree);
bool radix_tree_lookup(RadixTree *tree, uint16_t page_no);

// Walk while executing callback function - e.g., build freelist
void radix_tree_walk(RadixTree *tree, void (*cb)(uint16_t page, void* user_data), void* user_data);

// Conversion between freelist and radix tree
void freelist_to_radix(RadixTree* tree, uint16_t* freelist, size_t count);
size_t radix_to_freelist(RadixTree* tree, uint16_t* freelist, size_t max_size);
#endif
