// Radix tree is for finding and freeing free pages in O(log n) time
// This is used in a few parts of the code: 
// 1) Finding Free pages in Pager - For Reuse of pages, to prevent fragmentation
// 2) Free Pages in Journal if Transaction is used - For Reuse, such as when pages are dirty, we make a copy
// This is as pages in the database can get huge in number, and is dynamic in count. This job cannot be handled by just a bitmap alone

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define RADIX_BITS 4
#define RADIX_SIZE (1 << RADIX_BITS)

typedef struct RadixNode {
    struct RadixNode *children[RADIX_SIZE];  // next level
    int16_t status; // valid at leaves only (-1 = not free)
} RadixNode;

typedef struct RadixTree {
    RadixNode root;
} RadixTree;

RadixTree* radix_tree_create();
void radix_tree_destroy(RadixTree* tree);

// Basic Radix Tree operations
void radix_tree_insert(RadixTree *tree, uint16_t page_no, int16_t slot);
void radix_tree_delete(RadixTree *tree, uint16_t page_no);
int16_t radix_tree_pop_min(RadixTree *tree);
int16_t radix_tree_lookup(RadixTree *tree, uint16_t page_no);

// Walk while executing callback function - e.g free
void radix_tree_walk(RadixTree *tree, void (*cb)(uint16_t page, int16_t slot));

// Conversion between freelist and radix tree
void freelist_to_radix(RadixTree* tree, uint16_t* freelist, size_t count);
size_t radix_to_freelist(RadixTree* tree, uint16_t* freelist, size_t max_size);
