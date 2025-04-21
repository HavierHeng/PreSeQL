#ifndef PRESEQL_PAGER_DB_FREE_SPACE_H
#define PRESEQL_PAGER_DB_FREE_SPACE_H

#include "pager/types.h"
#include "status/db.h"


/* Manage Free Pages via Radix Tree 
 * Loads the free list from disk and builds a representation in Pager object memory using Radix Trees
 */
void init_free_page_map(Pager* pager);
void init_free_data_page_slots(Pager* pager);  // Variable size slots in Data Page
void init_overflow_data_page_slots(Pager* pager);  // Variable sized chunks/slots in Overflow

// Mark a page number as free
void mark_page_free(Pager* pager, uint16_t page_no);

// Mark a page number as allocated
void mark_page_used(Pager* pager, uint16_t page_no);

// Get a free page number, or return 0 if none 
// if no free page then use the highest known page number + 1
uint16_t get_free_page(Pager* pager);

// Sync free page map with the header's free page list
// This is called every time threshold, FREE_PAGE_LIST_BATCH_SIZE is met 
PSqlStatus sync_free_page_list(Pager* pager);

#endif /* PRESEQL_PAGER_DB_FREE_SPACE_H */

