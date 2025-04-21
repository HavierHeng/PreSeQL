#ifndef PRESEQL_PAGER_TYPES_H
#define PRESEQL_PAGER_TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "algorithm/radix_tree.h"
#include "pager/db/base/page.h"

/* Pager structure forward declaration same to avoid recursive imports */
typedef struct Pager Pager;

/* Free space management structures */
typedef enum {
    BUCKET_FULL,         // 0-25% free out of MAX_USABLE_PAGE_SIZE
    BUCKET_MOSTLY_FULL,  // 25-50% free out of MAX_USABLE_PAGE_SIZE
    BUCKET_MOSTLY_EMPTY, // 50-75% free out of MAX_USABLE_PAGE_SIZE
    // BUCKET_EMPTY is redundant - page would be marked as freed
} FreeSpaceBucket;

#define NUM_BUCKETS 3

typedef struct {
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceBuckets;

typedef struct {
    uint8_t num_frees;  // Counter for free increments whenever a slot is freed
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceTracker;

typedef struct {
    uint8_t num_frees;
    RadixTree tree;
} PageTracker;

/* Pager structures */
typedef struct {
    int fd;            // File descriptor for the database file
    void* mem_start;   // Start of memory-mapped region for database file
    size_t file_size;  // Size of the memory-mapped region
    PageTracker* free_page_map;   // Tracks free pages in a Radix Tree
    FreeSpaceTracker* free_data_page_slots;  // Variable size slots in Data Page
    FreeSpaceTracker* overflow_data_page_slots;  // Variable sized chunks/slots in Overflow
    // Index Pages doesn't need radix trees - searching free_slot_list[] enough
} DatabasePager;

typedef struct {
    int fd;             // File descriptor for the journal file
    void* mem_start;    // Start of memory-mapped region for journal file
    size_t file_size;   // Size of the memory-mapped region
    PageTracker* free_page_map;   // Tracks free pages in a Radix Tree
} JournalPager;

/* Pager structure definition */
struct Pager {
    char* filename;             // Database filename
    char* journal_filename;     // Journal filename
    DatabasePager db_pager;
    JournalPager journal_pager;
    uint8_t flags;              // Pager flags
    bool read_only;             // Whether the database is opened in read-only mode
};

/* Database handle structure */
typedef struct {
    char *filename;
    int flags;
    void *pager;  /* Pointer to pager implementation */
    char *error_msg;  /* In case opening the database has an error */
} PSql;

/* Page structures */
typedef struct {
    uint8_t slot_id;  // Up to 256 slots per page
    uint64_t offset;  // Offset to start of slot data
    uint64_t size;    // Size of slot data - same for all slots in the same page
} SlotEntry;

typedef struct {
    uint16_t next_page_id;  // Page of overflow page
    uint16_t next_chunk_id;  // Page of chunk in overflow page that contains more data
} OverflowPointer;

/* Index page structures */
typedef struct {
    uint8_t key[MAX_DATA_PER_INDEX_SLOT];   // Up to MAX_DATA_PER_INDEX_SLOT bytes of key
    uint16_t next_page_id;  // Pointer to next Index page
    uint8_t next_slot_id;   // Pointer to slot in next Index Page
    OverflowPointer overflow;  // Overflow pointer - null if no overflow
} IndexSlotData;

#endif /* PRESEQL_PAGER_TYPES_H */
