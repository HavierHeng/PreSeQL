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
#include "page.h"
#include "pager/pager_format.h"

/* Shared Flags values */
#define PAGE_INDEX_INTERNAL  = 0x01  // 0000 0001 - B+ Root or Internal Node Page. Internal nodes point to other Internal nodes or Leaf nodes.
#define PAGE_INDEX_LEAF      = 0x02  // 0000 0010 - B+ Leaf Node Page - We distinguish this to separate concerns since Leaf nodes point to Data Pages.
#define PAGE_DATA            = 0x04  // 0000 0100 - Data Page
#define PAGE_OVERFLOW        = 0x08  // 0000 1000 - Overflow Page
#define PAGE_DIRTY           = 0x10  // 0001 0000 - Page has been modified since the last sync or commit. In practice, this isn't needed since `msync` is done after all modifications.
#define PAGE_FREE            = 0x20  // 0010 0000 - Page is marked as free and can be reused. In practice, we don't use this since we have the Radix tree loaded in memory.
#define PAGE_COMPACTIBLE     = 0x40  // 0100 0000 - This flag indicates whether the slots in the page is eligible for compaction. Set when changes are made to the page, but unset after VACCUM. Can hint to page begin as compacted as it can be and should be skipped over during VACCUM.
#define PAGE_PINNED          = 0x80  // 1000 0000 - Page is pinned in memory can cannot be evicted - In practice, this isn't used since `mmap` deals with paging and caching on its own via the kernel.


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


/* Slot management functions - all types of Pages use slotted pages */
// Allocating slots



/* Slot cleaning - Vaccum - Applied to all pages as their slot entries can fragment as it is freed */
// TODO: Mark page free if reference counter drops to zero for a page - this is incremented/decremented by functions
void vaccum_page(DBPage);  // Vaccums page if flag COMPRESSABLE set and free_page < FREE_SPACE_VACCUM_SIZE - updates all slots but does not change their slot id

void vaccum_all_pages(Pager *pager);  // For loops over page 1 all the way to highest_page allocated in the db metadata - runs vaccum_page() on each one - this can be done at vm boot



#endif

