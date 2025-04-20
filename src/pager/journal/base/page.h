// Journal main header (Page 0)
#include <stdint.h>
#include "pager/constants.h"

typedef struct {
    char magic[MAGIC_NUMBER_SIZE];  // "SQLSHITE"
    uint32_t version;               // Journal format version
    uint32_t page_size;             // Page size (should match DB)
    uint32_t highest_txn_id;        // Highest transaction ID seen
    uint32_t checksum;              // CRC-32 of header
} JournalHeader;


