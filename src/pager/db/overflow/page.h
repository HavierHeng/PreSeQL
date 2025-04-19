/* Implements operations on overflow pages */

#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "../db_format.h"
#include "../../algorithm/radix_tree.h"

// Overflow pages store data in chunks. This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology
// Radix tree is used to implement a 4 bucket tracker for chunks. Example code is below - though this isn't correct and chunk size should be changed to whatever final chunks was choosen

// Overflow page operations
int allocate_overflow_chunk(Page* page, uint16_t size, uint8_t* chunk_id);
int write_to_overflow_chunk(Page* page, uint8_t chunk_id, const uint8_t* data, uint16_t size);
uint8_t* read_from_overflow_chunk(Page* page, uint8_t chunk_id, uint16_t* size);
int delete_overflow_chunk(Page* page, uint8_t chunk_id);

// Overflow allocator using radix tree
#define BUCKET_COUNT 4

typedef struct {
    RadixTree* free_buckets[BUCKET_COUNT];  // maps to page IDs
} OverflowPageAllocator;

// Initialize overflow allocator
OverflowPageAllocator* overflow_allocator_create();
void overflow_allocator_destroy(OverflowPageAllocator* alloc);

// See which bucket can still fit the overflow chunk
int get_bucket(size_t chunk_size);

// Add a page to the appropriate free bucket
void overflow_allocator_add_page(OverflowPageAllocator* alloc, uint16_t pg_id, size_t free_bytes);

// Get a page that can fit the needed bytes
uint16_t overflow_allocator_get_page(OverflowPageAllocator* alloc, size_t needed_bytes);

#endif /* PRESEQL_PAGER_DB_OVERFLOW_PAGE_H */

