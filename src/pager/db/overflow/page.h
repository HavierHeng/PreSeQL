/* Implements operations on overflow pages */

#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "pager/db/db_format.h"
#include "algorithm/radix_tree.h"

// Overflow pages store data in chunks. 
// This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology to keep things fresh

// Overflow page operations
int allocate_overflow_chunk(DBPage* page, uint16_t size, uint8_t* chunk_id);
int write_to_overflow_chunk(DBPage* page, uint8_t chunk_id, const uint8_t* data, uint16_t size);
uint8_t* read_from_overflow_chunk(DBPage* page, uint8_t chunk_id, uint16_t* size);
int delete_overflow_chunk(DBPage* page, uint8_t chunk_id);

#endif

