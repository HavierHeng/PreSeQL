/* Implements operations on overflow pages */

#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "../db_format.h"
#include "algorithm/radix_tree.h"

// Overflow pages store data in chunks. This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology

// Overflow page operations
int allocate_overflow_chunk(Page* page, uint16_t size, uint8_t* chunk_id);
int write_to_overflow_chunk(Page* page, uint8_t chunk_id, const uint8_t* data, uint16_t size);
uint8_t* read_from_overflow_chunk(Page* page, uint8_t chunk_id, uint16_t* size);
int delete_overflow_chunk(Page* page, uint8_t chunk_id);

// Note: At first I wanted to use a 4 bucket Radix tree to get the smallest chunk that fits. But thinking back, that is stupid, since the problem definition is that an overflow page has max 255 chunks.
// The cost of using a radix tree is lost since linear scan on 255 entries is fast, and the overhead of using radix tree means i have to copy to memory from disk. 
// I could just scan the chunk entries from disk instead.

#endif

