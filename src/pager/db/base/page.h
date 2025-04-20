/*
[ Page 0 ] Header Metadata - Stores magic number, versioning and roots of various tables - acts as a boot sector


| Field                   | Purpose                                 |
| ----------------------- | --------------------------------------- |
| Magic number            | File format/version check               |
| Page size               | 4KB or 8KB, etc                         |
| DB version              | Schema versioning or migrations         |
| Root pointers           | To tables/indexes (e.g., page numbers)  |
| Number of tables        | For iterating                           |
| Free page list          | Space management                        |
| highest_page            | Highest page allocated                  |
| Transaction state       | For WAL / crash recovery                |
| Optional flags          | E.g., read-only mode, corruption checks |
| Checksum                | Data corruption and recovery (CRC-32)   |
*/

#include <stdint.h>
#include "pager/constants.h"

// Database main header (Page 0)
typedef struct {
    char magic[MAGIC_NUMBER_SIZE];      // "SQLSHITE"
    uint16_t page_size;                 // Usually 4096
    uint16_t db_version;                // Schema version
    uint16_t root_table_catalog;        // Page 1
    uint16_t root_column_catalog;       // Page 2
    uint16_t root_fk_catalog;           // Page 3
    uint16_t free_page_list[FREE_PAGE_LIST_SIZE];       // Inline free page list
    uint16_t free_page_count;           // Number of entries in free_page_list - this allows us to pull off queue operations as it acts as an index
    uint16_t highest_page;              // Highest allocated page - allows for fall back if there are no free pages cached
    uint16_t transaction_state;         // For crash recovery
    uint16_t flags;                     // DB flags
    uint32_t checksum;                  // CRC-32 checksum
} DatabaseHeader;
