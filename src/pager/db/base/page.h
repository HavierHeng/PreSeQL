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

#ifndef PRESEQL_PAGER_DB_BASE_PAGE_H
#define PRESEQL_PAGER_DB_BASE_PAGE_H

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


// Generic Page Header - for Index, Data and Overflow pages
// Fun fact: Because all the pages are slotted page based - they actually share the same headers
// The only difference between page types is what is stored in each slot which does have some effect on compacting and accessing values in each type
typedef struct {
    // Page related
    uint16_t page_id;  // Max of 2^16 = 65535 pages
    uint16_t ref_counter;  // To know when free can be done
    uint8_t flag;  // See above for PAGE flags

    // Slot Related
    uint16_t free_start;  // Start of free space - does not exceed 16 bits - 4096 is 12 bits only
    uint16_t free_end;  // End of free space
    uint16_t free_total;  // Available free space to grow slots into
    uint8_t total_slots;  // How many slots are currently in use to now size of slot directory

    // For slot allocation
    uint8_t highest_slot;  // Fallback if no entries in free slot list
    uint8_t free_slot_count;  // For queue operations
    uint8_t free_slot_list[FREE_SLOT_LIST_SIZE];  // Track and reuse free slots as much as possible

    // B+ Tree specific 
    uint16_t right_sibling_page_id;  // Page ID of right sibling page for PAGE_INDEX_LEAF - set to NULL or ignore for PAGE_INDEX_INTERNAL, PAGE_DATA and PAGE_OVERFLOW

    // No need to pad to MAX_PAGE_HEADER_SIZE = 32 in bytes
} DBPageHeader;

// Page Memory, aligned to PAGE_SIZE (4096 Bytes usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
// data includes Slot directory + slot data entries + free space
typedef struct {
    DBPageHeader header;  // Padded to 32 bytes
    uint8_t data[MAX_USABLE_PAGE_SIZE];  // 4032 bytes - The data depends on the page type - its filled with SlotEntry and Index/Data/OverflowSlotData types
    uint8_t reserved[MAX_JOURNAL_HEADER_SIZE];  // 32 bytes - reserved for Journal later
} DBPage;

#endif /* PRESEQL_PAGER_DB_BASE_PAGE_H */

