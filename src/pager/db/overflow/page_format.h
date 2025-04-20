/* Overflow page and chunking */

#include "pager/constants.h"
#include <stdint.h>

typedef struct {
    uint8_t chunk_id;  // Unique chunk id to know what data is stored here
    uint8_t in_use;  // 1 if chunk is occupied, 0 if freed - can be used to VACCUM overflow pages
    uint16_t offset;   // Offset for where chunk begins
    uint16_t length;   // Chunk length
} OverflowChunkMeta;

typedef struct {
    uint8_t num_chunks;        // Allow up to 2^8=256 chunks per page
    uint16_t free_offset;      // Free offset is max 4096 = 12 bits to represent
    uint16_t free_bytes;       // Track half filled Overflow pages
    uint16_t payload_size;     // Total size of payload data
    uint16_t next_overflow_page; // Next overflow page in chain
    uint16_t reference_count;  /* To know if page can be marked as free. 
                                Reference counters increase if a new data page points to it. 
                                We know if a new data page references the overflow page 
                                since there is only one point of time where an overflow page can get new entries, when data pages are overflowed */
    uint8_t num_chunks_free;  // Count a threshold for chunk to be compacted via VACCUM
    OverflowChunkMeta chunk_table[MAX_OVERFLOW_CHUNKS];
} OverflowPageHeader;

