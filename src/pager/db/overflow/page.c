#include "page.h"
#include <string.h>

// Allocate a chunk in an overflow page
int allocate_overflow_chunk(Page* page, uint16_t size, uint8_t* chunk_id) {
    if (!page || !chunk_id || page->header.type != OVERFLOW) {
        return -1;
    }
    
    OverflowPageHeader* hdr = &page->header.type_specific.overflow;
    
    // Check if there's enough space
    if (hdr->free_bytes < size || hdr->num_chunks >= MAX_OVERFLOW_CHUNKS) {
        return -1;
    }
    
    // Allocate a new chunk
    *chunk_id = hdr->num_chunks;
    uint16_t offset = hdr->free_offset;
    
    // Update chunk table
    hdr->chunk_table[*chunk_id].chunk_id = *chunk_id;
    hdr->chunk_table[*chunk_id].offset = offset;
    hdr->chunk_table[*chunk_id].length = size;
    
    // Update header
    hdr->num_chunks++;
    hdr->free_offset += size;
    hdr->free_bytes -= size;
    hdr->payload_size += size;
    page->header.dirty = 1;
    
    return 0;
}

// Write data to an overflow chunk
int write_to_overflow_chunk(Page* page, uint8_t chunk_id, const uint8_t* data, uint16_t size) {
    if (!page || !data || page->header.type != OVERFLOW) {
        return -1;
    }
    
    OverflowPageHeader* hdr = &page->header.type_specific.overflow;
    
    // Check if chunk exists
    if (chunk_id >= hdr->num_chunks) {
        return -1;
    }
    
    // Check if size matches
    if (hdr->chunk_table[chunk_id].length != size) {
        return -1;
    }
    
    // Write data
    uint16_t offset = hdr->chunk_table[chunk_id].offset;
    memcpy(page->payload + offset, data, size);
    page->header.dirty = 1;
    
    return 0;
}

// Read data from an overflow chunk
uint8_t* read_from_overflow_chunk(Page* page, uint8_t chunk_id, uint16_t* size) {
    if (!page || !size || page->header.type != OVERFLOW) {
        return NULL;
    }
    
    OverflowPageHeader* hdr = &page->header.type_specific.overflow;
    
    // Check if chunk exists
    if (chunk_id >= hdr->num_chunks) {
        return NULL;
    }
    
    // Get chunk info
    uint16_t offset = hdr->chunk_table[chunk_id].offset;
    *size = hdr->chunk_table[chunk_id].length;
    
    return page->payload + offset;
}

// Delete an overflow chunk
int delete_overflow_chunk(Page* page, uint8_t chunk_id) {
    if (!page || page->header.type != OVERFLOW) {
        return -1;
    }
    
    OverflowPageHeader* hdr = &page->header.type_specific.overflow;
    
    // Check if chunk exists
    if (chunk_id >= hdr->num_chunks) {
        return -1;
    }
    
    // Get chunk info
    uint16_t size = hdr->chunk_table[chunk_id].length;
    
    // Mark chunk as deleted by setting length to 0
    hdr->chunk_table[chunk_id].length = 0;
    
    // Update header
    hdr->free_bytes += size;
    hdr->payload_size -= size;
    page->header.dirty = 1;
    
    // Note: We don't compact the page here, that would be done by a separate vacuum operation
    
    return 0;
}

// Initialize overflow allocator
OverflowPageAllocator* overflow_allocator_create() {
    OverflowPageAllocator* alloc = (OverflowPageAllocator*)malloc(sizeof(OverflowPageAllocator));
    if (!alloc) return NULL;
    
    // Initialize buckets
    for (int i = 0; i < BUCKET_COUNT; i++) {
        alloc->free_buckets[i] = radix_tree_create();
        if (!alloc->free_buckets[i]) {
            // Clean up previously allocated trees
            for (int j = 0; j < i; j++) {
                radix_tree_destroy(alloc->free_buckets[j]);
            }
            free(alloc);
            return NULL;
        }
    }
    
    return alloc;
}

// Destroy overflow allocator
void overflow_allocator_destroy(OverflowPageAllocator* alloc) {
    if (!alloc) return;
    
    // Free all buckets
    for (int i = 0; i < BUCKET_COUNT; i++) {
        if (alloc->free_buckets[i]) {
            radix_tree_destroy(alloc->free_buckets[i]);
        }
    }
    
    free(alloc);
}

// Determine which bucket can fit a chunk of given size
int get_bucket(size_t chunk_size) {
    if (chunk_size <= 512) return 0;
    if (chunk_size <= 1024) return 1;
    if (chunk_size <= 2048) return 2;
    if (chunk_size <= MAX_DATA_BYTES - sizeof(OverflowPageHeader)) return 3;
    return -1; // Too large
}

// Add a page to the appropriate free bucket
void overflow_allocator_add_page(OverflowPageAllocator* alloc, uint16_t pg_id, size_t free_bytes) {
    if (!alloc) return;
    
    int bucket = get_bucket(free_bytes);
    if (bucket >= 0 && bucket < BUCKET_COUNT) {
        radix_tree_insert(alloc->free_buckets[bucket], pg_id, (void*)1);
    }
}

// Get a page that can fit the needed bytes
uint16_t overflow_allocator_get_page(OverflowPageAllocator* alloc, size_t needed_bytes) {
    if (!alloc) return 0;
    
    int bucket = get_bucket(needed_bytes);
    if (bucket < 0) return 0; // Too large
    
    // Try to find a page in the appropriate bucket or larger
    for (int i = bucket; i < BUCKET_COUNT; i++) {
        int16_t pg_id = radix_tree_pop_min(alloc->free_buckets[i]);
        if (pg_id > 0) {
            return pg_id;
        }
    }
    
    return 0; // No suitable page found
}
