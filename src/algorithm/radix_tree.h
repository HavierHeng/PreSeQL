// Radix tree is for finding and freeing free pages in O(nlogn) time
// This is used in a few parts of the code: 
// 1) Finding Free pages in Pager - For Reuse of pages, to prevent fragmentation
// 2) Free Pages in Journal if Transaction is used - For Reuse, such as when pages are dirty, we make a copy
// This is as pages in the database can get huge in number, and is dynamic in count. This job cannot be handled by just a bitmap alone

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define RADIX_BITS 8
#define RADIX_SIZE (1 << RADIX_BITS)


typedef struct RadixNode {
    struct RadixNode *children[RADIX_SIZE];  // next level
    int32_t status; // valid at leaves only (-1 = not free)
} RadixNode;

typedef struct RadixTree {
    RadixNode root;
} RadixTree;

RadixTree* radix_tree_create();

// Basic Radix Tree operations
void radix_tree_insert(RadixTree *tree, uint32_t page_no, int32_t slot);
void radix_tree_delete(RadixTree *tree, uint32_t page_no);
int32_t radix_tree_pop_min(RadixTree *tree);
int32_t radix_tree_lookup(RadixTree *tree, uint32_t page_no);

// Walk while executing callback function - e.g free
void radix_tree_walk(RadixNode *node, uint32_t prefix, int depth, void (*cb)(uint32_t page, int slot));
