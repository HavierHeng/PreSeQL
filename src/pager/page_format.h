#ifndef PRESEQL_PAGER_PAGE_FORMAT_H
#define PRESEQL_PAGER_PAGE_FORMAT_H

#include <stdint.h>
#include "config.h"
#include "db/index/page_format.h"
#include "db/data/page_format.h"
#include "db/overflow/page_format.h"

// Forward declarations
typedef struct Page Page;

// Page types
typedef enum {
    DATA,
    INDEX,
    OVERFLOW
} PageType;

// Header different for each of 3 Page types 
typedef union {
    BTreePageHeader btree;
    DataPageHeader data;
    OverflowPageHeader overflow;
} PageTypeHeader;

// Generic Page Header
typedef struct {
    uint16_t page_id;  // Max of 2^16 = 65535 pages
    uint8_t dirty;
    uint8_t free;
    PageType type;
    PageTypeHeader type_specific;
} PageHeader;

// Page Memory, aligned to PAGE_SIZE (4KB usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
typedef struct Page {
    PageHeader header;
    uint8_t payload[MAX_DATA_BYTES];
} Page;

// Database header (Page 0)
typedef struct {
    char magic[MAGIC_NUMBER_SIZE];      // "SQLSHITE"
    uint16_t page_size;                 // Usually 4096
    uint16_t db_version;                // Schema version
    uint16_t root_table_catalog;        // Page 1
    uint16_t root_column_catalog;       // Page 2
    uint16_t root_fk_catalog;           // Page 3
    uint16_t free_page_list[256];       // Inline free page list
    uint16_t free_page_count;           // Number of entries in free_page_list
    uint16_t highest_page;              // Highest allocated page
    uint16_t transaction_state;         // For crash recovery
    uint16_t flags;                     // DB flags
    uint32_t checksum;                  // CRC-32 checksum
} DatabaseHeader;

#endif /* PRESEQL_PAGER_PAGE_FORMAT_H */
