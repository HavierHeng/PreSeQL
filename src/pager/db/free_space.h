#include "algorithm/radix_tree.h"

/* Radix Tree free buckets - to determine page has slots to put things in 
 * BUCKET_FULL,  // 0-25% free out of MAX_USABLE_PAGE_SIZE
 * BUCKET_MOSTLY_FULL,  // 25-50% free out of MAX_USABLE_PAGE_SIZE
 * BUCKET_MOSTLY_EMPTY,  // 50-75% free out of MAX_USABLE_PAGE_SIZE
 * */
#define NUM_BUCKETS 3

typedef enum {
    BUCKET_FULL,  // 0-25% free out of MAX_USABLE_PAGE_SIZE
    BUCKET_MOSTLY_FULL,  // 25-50% free out of MAX_USABLE_PAGE_SIZE
    BUCKET_MOSTLY_EMPTY,  // 50-75% free out of MAX_USABLE_PAGE_SIZE
    // BUCKET_EMPTY  // Redundant - if the bucket is empty (e.g after deletion of entries in index/data page), reference count drops to 0 and the page would be marked as freed (adding it back to the free page tracker)
} FreeSpaceBucket;

typedef struct {
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceBuckets;  // Just the 4 RadixTrees, no buckets - Not really used, since its more useful to be able to track and batch free

typedef struct {
    uint8_t num_frees;  // Counter for free increments whenever a slot is freed - as freeing slots make slot holes, and holes == wasted space
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceTracker;  // Only Data and Overflow pages use this - this is as their slots have variable sizes given it stores data from all kinda of places so its not sufficient to just track free slots

typedef struct {
    uint8_t num_frees;
    RadixTree tree;
} PageTracker;  // Radix Tree + Free count - this is used for tracking free pages - this might be more useful


/* Manage Free Pages via Radix Tree 
 * Loads the free list from disk and builds a representation in Pager object memory using Radix Trees
 * */
void init_free_page_map(Pager* pager);
void init_free_data_page_slots(Pager* pager);  // Variable size slots in Data Page
void init_overflow_data_page_slots(Pager* pager);  // Variable sized chunks/slots in Overflow

// Mark a page number as free
void mark_page_free(Pager* pager, uint16_t page_no);

// Mark a page number as allocated
void mark_page_used(Pager* pager, uint16_t page_no);

// Get a free page number, or return 0 if none 
// if no free page then use the highest known page number + 1 (stored in page header) - increment the highest allocated page number by 1 as well
uint16_t get_free_page(Pager* pager);

// Sync free page map with the header's free page list
// This is called every time threshold, FREE_PAGE_LIST_BATCH_SIZE is met 
// this represents the number of changes to global free_page_map before flushing back to disk free page list using the radix_tree helper function
PSqlStatus sync_free_page_list(Pager* pager);

