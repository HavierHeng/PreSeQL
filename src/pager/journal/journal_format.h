#ifndef PRESEQL_PAGER_JOURNAL_FORMAT_H
#define PRESEQL_PAGER_JOURNAL_FORMAT_H

#include <stdint.h>
#include "../config.h"

// Journal file header
typedef struct {
    char magic[MAGIC_NUMBER_SIZE];  // "SQLSHITE"
    uint32_t version;               // Journal format version
    uint32_t page_size;             // Page size (should match DB)
    uint32_t highest_txn_id;        // Highest transaction ID seen
    uint32_t checksum;              // CRC-32 of header
} JournalHeader;

#endif /* PRESEQL_PAGER_JOURNAL_FORMAT_H */
