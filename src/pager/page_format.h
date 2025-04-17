/* Define page structures and headers format */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define PAGE_SIZE 4096
#define JOURNAL_META_SIZE 12 // Journal header: txn_id, page_no, etc.
#define MAX_DATA_BYTES (PAGE_SIZE - JOURNAL_META_SIZE) // Limit usable bytes for data in data pages, since journal has some overhead itself, but we aim for page alignment

typedef enum {
    DATA,
    INDEX,
    OVERFLOW
} PageType;

typedef enum {
    BTREE_ROOT = 1,
    BTREE_INTERIOR,
    BTREE_LEAF
} BTreePageType;

// TODO : all B+ Tree Leafs are linked in order of the primary key to allow linear traversal - so there should be a next pointer to another B+ Tree Leaf
typedef struct {
    uint32_t page_id;
    BTreePageType btree_type;
    uint16_t free_start;
    uint16_t free_end;
    uint16_t total_free;
    uint8_t flags;
} BTreePageHeader;

typedef struct {
    uint16_t num_slots;
    uint16_t slot_directory_offset;
} DataPageHeader;

typedef struct {
    uint32_t next_overflow_page;
    uint16_t payload_size;
    uint16_t reserved;
} OverflowPageHeader;

// Header different for each of 3 Page types 
typedef union {
    BTreePageHeader btree;
    DataPageHeader data;
    OverflowPageHeader overflow;
} PageTypeHeader;

// Generic Page Header
typedef struct {
    uint8_t dirty;
    uint8_t free;
    PageType type;
    PageTypeHeader type_specific;
} PageHeader;


// Page Memory, aligned to PAGE_SIZE (4KB usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
typedef struct {
    PageHeader header;
    uint8_t payload[MAX_DATA_BYTES];  // Slotted page or B+ nodes
} Page;

