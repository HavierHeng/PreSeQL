/* Implements operations on overflow pages */

#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "pager/db/base/page_format.h"  // DBPage, DBPageHeader, SlotEntry, OverflowPointer
#include "pager/constants.h"
#include "pager/pager.h"  // Pager

// Overflow pages store data in chunks. 
// This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology to keep things fresh
typedef struct {
    uint8_t data[MAX_USABLE_PAGE_SIZE];
    OverflowPointer overflow;
} Chunk;

// Manipulating chunks - Overflow
// Chunks still follow the slotted page format - its just a semantics thing

init_overflow_page(Pager* pager, uint16_t page_no);
uint8_t find_empty_overflow_slot(Pager* pager, uint64_t value_size);  // Finds a slot in overflow pages that can fit the value - i.e a chunk via searching the radix buckets
// Overflow chunks do not need to be order in any ways, any candidate with enough free space will do
void read_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Chunk* chunk);
void write_overflow_slot(Pager* pager, Chunk *chunk);  // Pager doesn't need to slot in any particular data slot - only condition is that the page has enough free space - yes this will lead to inefficiency of page accesses since i mix data but screw it
void free_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);

#endif

