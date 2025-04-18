/* Overflow page and radix tree allocator */

#define MAX_OVERFLOW_CHUNKS 256  // Max chunks that can be stored in an Overflow page
//
typedef struct {
    uint8_t chunk_id;  // Chunk
    uint16_t offset;  // Offset for where chunk begins
    uint16_t length;  // Chunk length
} OverflowChunkMeta;

typedef struct {
    uint8_t num_chunks;  // allow up to 2^8=64 chunks per page
    uint16_t free_offset;  // Free offset is max 4096 = 10 bits to represent
    uint16_t free_bytes;  // track half filled Overflow pages

    uint32_t next_overflow_page;
    OverflowChunkMeta chunk_table[MAX_OVERFLOW_CHUNKS];
    uint8_t data[];  // TODO: What size?
} OverflowPageHeader;

