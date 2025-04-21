#include "pager/db/base/page_format.h"
#include "pager/constants.h"
#include "pager/pager.h"

/* For both Internal and Leaf Index Pages */
typedef struct {
    uint8_t key[MAX_DATA_PER_INDEX_SLOT];   // Up to MAX_DATA_PER_INDEX_SLOT (16) bytes of truncated key - key are encoded specially to be lexicographically sortable
    uint16_t next_page_id;  // Pointer to next Index page (Internal/leaf) that might contain data
    uint8_t next_slot_id;  // Pointer to slot in next Index Page - Up to 256 slots per page
    OverflowPointer overflow;  // Overflow pointer - null if no overflow
} IndexSlotData;


init_index_internal_page(Pager* pager, uint16_t page_no);
init_index_leaf_page(Pager* pager, uint16_t page_no);

// Manipulating slots - Index
// TODO: Should this write to a new struct? And also the slot number
uint8_t find_empty_index_slot(Pager* pager, uint16_t page_id, uint64_t key_size);  // Since Index pages have an order - its more useful to search for a slot in a specific page for building B+ tree. If the page is not a leaf or internal node, returns 0. Pick slot from free slot list else fallback to highest slot allocated. 
void read_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, IndexSlotData *slot);  // Parse the slot data for use for comparisons
void write_index_slot(Pager* pager, uint16_t page_id, IndexSlotData *slot); // Pick slot from free_slot_list else fallback to highest. Copies in slot data and sets up a slot entry in the front. Insert slot in order using binary search to find where it should insert at (shift right entries if needed). Updates page header. updates page header as well.
void free_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id); 




