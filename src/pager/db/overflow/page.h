/* Implements operations on overflow pages */

#ifndef PRESEQL_PAGER_DB_OVERFLOW_PAGE_H
#define PRESEQL_PAGER_DB_OVERFLOW_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "../db_format.h"
#include "../../algorithm/radix_tree.h"

/* TODO:
 * Data operations:
 * 1) In reality, due to overflow pages, pulling out data is harder than it sounds. it might require the following of a linked list style overflow page. Overflow pages themselves also have chunks, i.e multiple data pages might store chunks of their data inside an overflow page. So you need to way to follow through each overflow page
*/

// Overflow pages store data in chunks. This sounds pretty much like slotted pages, because it technically is implemented exactly the same way. Im just using different terminology

// TODO: Use the radix tree to implement a 4 bucket tracker for chunks. Example code is below - though this isn't correct and chunk size should be changed to whatever final chunks was choosen
/*
*
* #define BUCKET_COUNT 4

// Page is free for overflow chunking and has X bytes available
typedef struct {
    RadixTree free_buckets[BUCKET_COUNT];  // maps to page IDs
} OverflowPageAllocator;


// See which bucket can still fit the overflow chunk
int get_bucket(size_t chunk_size) {
    if (chunk_size <= 511) return 1;
    if (chunk_size <= 1023) return 2;
    if (chunk_size <= 2047) return 3;
    return -1; // too large, allocate new page
}

// On insert into overflow page - update page's free_bytes, re-bucket it by removing from old bucket, and insert into new bucket

// On page full - remove page from all buckets, will not be reused until a chunk is deleted

// On chunk delete - increase free_bytes on page, re-insert page into appropraite free bucket (optional: compact chunks to reduce fragmentation)

void overflow_allocator_add_page(OverflowPageAllocator *alloc, PageID pg_id, size_t free_bytes) {
    int bucket = get_bucket(free_bytes);
    if (bucket >= 0)
        radix_insert(&alloc->free_buckets[bucket], pg_id);
}

PageID overflow_allocator_get_page(OverflowPageAllocator *alloc, size_t needed_bytes) {
    int bucket = get_bucket(needed_bytes);
    for (int i = bucket; i < BUCKET_COUNT; i++) {
        PageID pg = radix_get_any(&alloc->free_buckets[i]);  // any page in this bucket
        if (pg != 0) return pg;
    }
    return 0; // none found
}

*
*/

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

