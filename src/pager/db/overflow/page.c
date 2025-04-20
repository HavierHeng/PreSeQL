#include "page_format.h"
#include <string.h>

// TODO: Only for reference - I'll remake this outside

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

// Vaccum fragmented chunks
void vacuum_overflow_page(Page* page) {
    OverflowPageHeader* hdr = (OverflowPageHeader*)page->data;
    uint8_t* base = page->data;

    // Temporary buffer to store the compacted chunk data
    uint8_t compacted[PAGE_SIZE];

    // Start writing data just after the header
    size_t header_size = offsetof(OverflowPageHeader, chunk_table[MAX_OVERFLOW_CHUNKS]);
    uint16_t new_offset = header_size;

    // New chunk table to build into
    OverflowChunkMeta new_chunks[MAX_OVERFLOW_CHUNKS];
    memset(new_chunks, 0, sizeof(new_chunks));

    uint16_t total_payload = 0;
    uint8_t new_num_chunks = 0;

    for (int i = 0; i < MAX_OVERFLOW_CHUNKS; ++i) {
        OverflowChunkMeta* old_chunk = &hdr->chunk_table[i];
        if (!old_chunk->in_use) continue;

        // Copy chunk data into new compacted location
        uint8_t* src = base + old_chunk->offset;
        memcpy(compacted + new_offset, src, old_chunk->length);

        // Build new chunk entry
        new_chunks[i].in_use = 1;
        new_chunks[i].offset = new_offset;
        new_chunks[i].length = old_chunk->length;

        new_offset += old_chunk->length;
        total_payload += old_chunk->length;
        new_num_chunks++;
    }

    // Copy compacted data back into original page
    memcpy(base + header_size, compacted + header_size, total_payload);

    // Write back the updated chunk table
    memcpy(hdr->chunk_table, new_chunks, sizeof(new_chunks));

    // Update header metadata
    hdr->free_offset = new_offset;
    hdr->free_bytes = PAGE_SIZE - new_offset;
    hdr->payload_size = total_payload;
    hdr->num_chunks = new_num_chunks;

    // Count freed chunk slots
    uint8_t free_count = 0;
    for (int i = 0; i < MAX_OVERFLOW_CHUNKS; ++i) {
        if (!hdr->chunk_table[i].in_use) free_count++;
    }
    hdr->num_chunks_free = free_count;
}

