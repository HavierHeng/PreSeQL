/* Implements operations on overflow pages */
#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "pager/types.h"
#include "pager/constants.h"
#include "pager/pager.h"  // Pager
#include "pager/db/free_space.h"

// Overflow pages store data in chunks. 
// This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology to keep things fresh
typedef struct {
    uint8_t data[MAX_USABLE_PAGE_SIZE];
    OverflowPointer overflow;
} Chunk;

// Manipulating chunks - Overflow
// Chunks still follow the slotted page format - its just a semantics thing
// void init_overflow_page(Pager* pager, uint16_t page_no);
uint8_t find_empty_overflow_slot(Pager* pager, uint16_t page_id, size_t data_size);  // Finds a slot in overflow pages that can fit the value - i.e a chunk via searching the radix buckets
// Overflow chunks do not need to be order in any ways, any candidate with enough free space will do
void read_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Chunk* chunk);

PSqlStatus read_overflow_data(Pager* pager, uint16_t page_id, uint16_t chunk_id, uint8_t* data, uint16_t* size);

void free_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);

PSqlStatus write_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, const uint8_t* data, size_t data_size, OverflowPointer* overflow);  // Pager doesn't need to slot in any particular data slot - only condition is that the page has enough free space - yes this will lead to inefficiency of page accesses since i mix data but screw it

PSqlStatus write_overflow_data(Pager* pager, uint16_t page_id, const uint8_t* data, size_t size, uint16_t* chunk_id);

#endif

