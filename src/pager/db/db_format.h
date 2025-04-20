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

    // Pad to MAX_PAGE_HEADER_SIZE = 32 in bytes
    uint8_t reserved[2];
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


void find_slot_in_page();  // Finds the slot of slot_id in the page of page_id

void read_index_slot(uint8_t *slot_ptr, IndexSlotData *slot);  // Parse the slot data for use for comparisons
void write_index_slot(uint8_t *slot_ptr, IndexSlotData *slot); // Pick slot from free_slot_list else fallback to highest. Copies in slot data and sets up a slot entry in the front. Updates page header.
void free_index_slot(uint8_t *slot_ptr, IndexSlotData *slot); 

//TODO: Repeat for each slot type



// TODO: B+ tree operations: See index/page.c
void encode_index_key();  // B+ Tree - applies the encoding trick to signed int by adding 2^63, NULL should be compared first
void compare_index_key();  // B+ Tree - compares keys lexicographically

// TODO: Compose insert_row and delete_row, get_row using B+ tree and the index, overflow and data page operations

/* Usage in practice
// Usage - creating a data page like this basically
// Creates a new B+ Tree node (page)
// Insert stuff - in memory this is a slotted row
// In practice, if you have journalling - this is where you should make use of it to write to a journal
//
Page* p = init_data_page(100);
uint8_t row1[] = { 1, 2, 3, 4 };
insert_row(p, row1, sizeof(row1));

uint16_t row_size;
uint8_t* r = get_row(p, 0, &row_size);
delete_row(p, 0);
*/
#endif

