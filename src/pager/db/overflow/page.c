#include "page_format.h"
#include <string.h>

// TODO: Only for reference - I'll remake this outside
// allocate_overflow_chunk()
// read_from_overflow_chunk()
// vaccum_overflow_chunk() - no need making vaccum_page() would be more efficient given they share the same header

int allocate_overflow_chunk(Page* page, uint16_t size, uint8_t* chunk_id) {
    OverflowPageHeader* hdr = (OverflowPageHeader*)page->data;

    // Not enough space in the overflow page?
    if (hdr->free_bytes < size || hdr->num_chunks >= MAX_OVERFLOW_CHUNKS) {
        return -1;
    }

    // Find a free chunk slot
    for (int i = 0; i < MAX_OVERFLOW_CHUNKS; ++i) {
        if (!hdr->chunk_table[i].in_use) {
            if (hdr->free_offset + size > page->size) {  // Safety check
                return -1;
            }

            // Set up the chunk
            OverflowChunkMeta* chunk = &hdr->chunk_table[i];
            chunk->in_use = 1;
            chunk->offset = hdr->free_offset;
            chunk->length = size;

            // Update header metadata
            hdr->free_offset += size;
            hdr->free_bytes -= size;
            hdr->payload_size += size;
            hdr->num_chunks++;
            hdr->num_chunks_free--; // If reusing a previously freed chunk slot

            *chunk_id = i;
            return 0;
        }
    }

    return -1; // No free slot found
}

uint8_t* read_from_overflow_chunk(Page* page, uint8_t chunk_id, uint16_t* size_out) {
    OverflowPageHeader* hdr = (OverflowPageHeader*)page->data;

    if (chunk_id >= MAX_OVERFLOW_CHUNKS) return NULL;

    OverflowChunkMeta* chunk = &hdr->chunk_table[chunk_id];
    if (!chunk->in_use) return NULL;

    if (size_out) {
        *size_out = chunk->length;
    }

    return page->data + chunk->offset;
}


