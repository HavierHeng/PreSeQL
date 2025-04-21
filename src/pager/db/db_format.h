/* 
 * Collate page structures and headers format 
 * */

#ifndef PRESEQL_PAGER_DB_FORMAT_H
#define PRESEQL_PAGER_DB_FORMAT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pager/constants.h"
#include "pager/pager.h"
#include "pager/db/base/page.h"  // Page 0
#include "types/psql_types.h"

/* Shared Flags values */
#define PAGE_INDEX_INTERNAL  = 0x01  // 0000 0001 - B+ Root or Internal Node Page. Internal nodes point to other Internal nodes or Leaf nodes.
#define PAGE_INDEX_LEAF      = 0x02  // 0000 0010 - B+ Leaf Node Page - We distinguish this to separate concerns since Leaf nodes point to Data Pages.
#define PAGE_DATA            = 0x04  // 0000 0100 - Data Page
#define PAGE_OVERFLOW        = 0x08  // 0000 1000 - Overflow Page
#define PAGE_DIRTY           = 0x10  // 0001 0000 - Page has been modified since the last sync or commit. In practice, this isn't needed since `msync` is done after all modifications.
#define PAGE_FREE            = 0x20  // 0010 0000 - Page is marked as free and can be reused. In practice, we don't use this since we have the Radix tree loaded in memory.
#define PAGE_COMPACTIBLE     = 0x40  // 0100 0000 - This flag indicates whether the slots in the page is eligible for compaction. Set when changes are made to the page, but unset after VACCUM. Can hint to page begin as compacted as it can be and should be skipped over during VACCUM.
#define PAGE_PINNED          = 0x80  // 1000 0000 - Page is pinned in memory can cannot be evicted - In practice, this isn't used since `mmap` deals with paging and caching on its own via the kernel.

// Generic Page Header - for Index, Data and Overflow pages
// Fun fact: Because all the pages are slotted page based - they actually share the same headers
// The only difference between page types is what is stored in each slot which does have some effect on compacting and accessing values in each type
typedef struct {
    // Page related
    uint16_t page_id;  // Max of 2^16 = 65535 pages
    uint16_t ref_counter;  // To know when free can be done
    uint8_t flag;  // See above for PAGE flags

    // Slot Related
    uint16_t free_start;  // Start of free space - does not exceed 16 bits - 4096 is 12 bits only
    uint16_t free_end;  // End of free space
    uint16_t free_total;  // Available free space to grow slots into
    uint8_t total_slots;  // How many slots are currently in use to now size of slot directory

    // For slot allocation
    uint8_t highest_slot;  // Fallback if no entries in free slot list
    uint8_t free_slot_count;  // For queue operations
    uint8_t free_slot_list[FREE_SLOT_LIST_SIZE];  // Track and reuse free slots as much as possible

    // B+ Tree specific 
    uint16_t right_sibling_page_id;  // Page ID of right sibling page for PAGE_INDEX_LEAF - set to NULL or ignore for PAGE_INDEX_INTERNAL, PAGE_DATA and PAGE_OVERFLOW

    // No need to pad to MAX_PAGE_HEADER_SIZE = 32 in bytes
} DBPageHeader;

// Page Memory, aligned to PAGE_SIZE (4096 Bytes usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
// data includes Slot directory + slot data entries + free space
typedef struct {
    DBPageHeader header;  // Padded to 32 bytes
    uint8_t data[MAX_USABLE_PAGE_SIZE];  // 4032 bytes - The data depends on the page type - its filled with SlotEntry and Index/Data/OverflowSlotData types
    uint8_t reserved[MAX_JOURNAL_HEADER_SIZE];  // 32 bytes - reserved for Journal later
} DBPage;


/* Stored in slot directory table after Header */
typedef struct {
    uint8_t slot_id;  // Up to 256 slots per page
    uint64_t offset;  // Offset to start of slot data
    uint64_t size;  // Size of slot data - same for all slots in the same page
} SlotEntry;


typedef struct {
    uint16_t next_page_id;  // Page of overflow page
    uint16_t next_chunk_id;  // Page of chunk in overflow page that contains more data
} OverflowPointer;

/* The only difference between page types is the Slot data they store 
 * No need define structs for these - just implement functions that know how to read in the format
 * */

/* For both Internal and Leaf Index Pages */
typedef struct {
    uint8_t key[MAX_DATA_PER_INDEX_SLOT];   // Up to MAX_DATA_PER_INDEX_SLOT (16) bytes of truncated key - key are encoded specially to be lexicographically sortable
    uint16_t next_page_id;  // Pointer to next Index page (Internal/leaf) that might contain data
    uint8_t next_slot_id;  // Pointer to slot in next Index Page - Up to 256 slots per page
    OverflowPointer overflow;  // Overflow pointer - null if no overflow
} IndexSlotData;

/* For Data Page */
typedef struct {
    uint8_t data[MAX_DATA_PER_DATA_SLOT];  // Up to MAX_DATA_PER_DATA_SLOT (255) bytes of truncated data - the name is different to reflect that values are stored as is
    OverflowPointer overflow;  // Overflow pointer - if row cannot fit within the data given
} DataSlotData;

/* For Overflow Page chunks - Still a slotted page */
typedef struct {
    uint8_t data[MAX_USABLE_PAGE_SIZE];  // Can take up as much as the whole overflow chunk
    OverflowPointer overflow;  // If still overflowed
} OverflowSlotData;



/* Slot management functions - all types of Pages use slotted pages */
// Allocating slots

// Manipulating slots - Index
// TODO: Should this write to a new struct? And also the slot number
uint8_t find_empty_index_slot(Pager* pager, uint16_t page_id, uint64_t key_size);  // Since Index pages have an order - its more useful to search for a slot in a specific page for building B+ tree. If the page is not a leaf or internal node, returns 0. Pick slot from free slot list else fallback to highest slot allocated. 
void read_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, IndexSlotData *slot);  // Parse the slot data for use for comparisons
void write_index_slot(Pager* pager, uint16_t page_id, IndexSlotData *slot); // Pick slot from free_slot_list else fallback to highest. Copies in slot data and sets up a slot entry in the front. Insert slot in order using binary search to find where it should insert at (shift right entries if needed). Updates page header. updates page header as well.
void free_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id); 

// Manipulating slots - Data
typedef struct {
    PSqlDataTypes type;
    uint8_t* data;
    uint64_t size;
    OverflowPointer* overflow;  // NULL if no overflow
} Cell;  // Single column value in a returned row

typedef struct {
    uint32_t num_columns;
    Cell* cells;
} Row;  // A whole row of data

uint8_t find_empty_data_slot(Pager* pager, uint64_t value_size);  // Finds a slot in data pages that can fit the value - i.e a row of data via searching the radix buckets
// Data slots do not need to be ordered in any way, so any candidate with space will do
void read_data_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Row *row);
void write_data_slot(Pager* pager, Row *row);  // Pager doesn't need to slot in any particular data slot - only condition is that the page has enough free space - yes this will lead to inefficiency of page accesses since i mix data but screw it
void free_data_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);


// Manipulating chunks - Overflow
// TODO: I think i need an overflow data type maybe e.g Chunk

uint8_t find_empty_overflow_slot(Pager* pager, uint64_t value_size);  // Finds a slot in overflow pages that can fit the value - i.e a chunk via searching the radix buckets
// Overflow chunks do not need to be order in any ways, any candidate with enough free space will do
void read_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Chunk* chunk);
void write_overflow_slot(Pager* pager, Chunk *chunk);  // Pager doesn't need to slot in any particular data slot - only condition is that the page has enough free space - yes this will lead to inefficiency of page accesses since i mix data but screw it
void free_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);



/* Slot cleaning - Vaccum - Applied to all pages as their slot entries can fragment as it is freed */
// TODO: Mark page free if reference counter drops to zero for a page - this is incremented/decremented by functions
void vaccum_page(DBPage);  // Vaccums page if flag COMPRESSABLE set and free_page < FREE_SPACE_VACCUM_SIZE - updates all slots but does not change their slot id

void vaccum_all_pages(Pager *pager);  // For loops over page 1 all the way to highest_page allocated in the db metadata - runs vaccum_page() on each one - this can be done at vm boot

void vaccum_data_pages(FreeSpaceTracker* tracker);  // Vaccum only on 4-bucket radix tree with tracker - this is done when the database is online - i.e only data and overflow pages get this treatment


/* B+ Tree operations - Technically all Page types are part and parcel of B+ trees, Internal index and interal leaf just deal with routing and data pointing respectively */
btree_init();  // Create a new B+ Tree index with a primary key. get_free_page() to get a page, init an index internal page to spawn a B+ tree root. Put the entry into the table catalog.
btree_destroy();  // recursively mark all child nodes as freed by walking through the tree, while adding these changes to the free page radix - increment global count for freed pages. For data and overflow pages, decrement reference counts, clear the slots if unused. If data/overflow page has 0 references left, free apge. Else if still have reference, vaccum and redistribute buckets
btree_search();  // use compare_prefix and size of keys to compare in order 
btree_split_leaf(left, right);  // top-down logic - parent initiates the split rather than child
btree_split_internal(left, right);  // Same
btree_insert();
btree_delete(); 
BTreeIterator* btree_iterator_range(Pager *pager, start_key, end_key);  // Returns multiple Row results that can be stepped through
Row* btree_iterator_next(BPlusIterator* it);

uint64_t encode_int_key(int64_t key);  // For lexicographic comparison. Encodes signed int64_t values and encodes it by adding an offset of 2^63 to remap the range to a positive unsigned value for comparison. Uses XOR trick to x^0x80000

compare_prefix();  // compares keys lexicographically, chases overflow pages if its available

compare();

/* TODO: Database level B+ Tree operations - Insert row, delete_row, get_row*/


#endif

