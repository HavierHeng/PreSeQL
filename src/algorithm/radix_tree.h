// Radix tree is for finding and freeing free pages in O(nlogn) time
// This is used in a few parts of the code: 
// 1) Finding Free pages in Pager - For Reuse
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


void radix_insert(RadixNode *root, uint32_t page_num, int32_t slot);
int32_t radix_lookup(RadixNode *root, uint32_t page_num);
void walk_radix(RadixNode *node, uint32_t prefix, int depth, void (*cb)(uint32_t page, int slot));
