/* Aka the serializer, handles read/writes to disk for database and journal. 
 *
[ Page 0 ] Header Metadata - Stores magic number, versioning and roots of various tables - acts as a boot sector

[ Page 1 ] Table Catalog (B+ Tree Root Page) - track each Index Page for each table, its names and its root page and Index type

[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table

[ Page 3 ] Foreign Key Catalog - track Foreign Key Constraints between table to table
  
[ Page 4..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ tree nodes) - Internal nodes, and leaf nodes which point to data (or other nodes)
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit within a page
  └─ Free Pages - Pages marked as freed in database file, these are holes created after deletion of pages (a bitmap in the Page 0 header on disk keeps track of what pages are free for reuse - to fill in holes)
 * */

#ifndef PRESEQL_PAGER_H
#define PRESEQL_PAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>  // For specifying mmap() flags
#include <sys/stat.h>  // Handle structs of data by stat() functions
#include "constants.h"
#include "algorithm/radix_tree.h"  // For efficiently finding free pages
#include "algorithm/crc.h"  // For getting CRC32 checksum to check header validity
#include "pager/db/db_format.h"
#include "preseql.h"  // Implement PSql Open and Close methods
#include "constants.h"

/* Database handle structure */
typedef struct {
    char *filename;
    int flags;
    void *pager;  /* Pointer to pager implementation - giving it access to the underlying files for DB and Journal */
    char *error_msg;  /* In case opening the database has an error */
} PSql;

typedef struct {
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceBuckets;  // Just the 4 RadixTrees, no buckets - Not really used, since its more useful to be able to track and batch free

typedef struct {
    uint8_t num_frees;  // Counter for free increments whenever a slot is freed - as freeing slots make slot holes, and holes == wasted space
    RadixTree buckets[NUM_BUCKETS];
} FreeSpaceTracker;  // Only Data and Overflow pages use this - this is as their slots have variable sizes given it stores data from all kinda of places so its not sufficient to just track free slots

typedef struct {
    uint8_t num_frees;
    RadixTree tree;
} PageTracker;  // Radix Tree + Free count - this is used for tracking free pages - this might be more useful

typedef enum {
    BUCKET_FULL,
    BUCKET_MOSTLY_FULL,
    BUCKET_MOSTLY_EMPTY,
    // BUCKET_EMPTY  // Redundant - if the bucket is empty (e.g after deletion of entries in index/data page), reference count drops to 0 and the page would be marked as freed (adding it back to the free page tracker)
} FreeSpaceBucket;

/* Separating concerns for the two Pagers */
typedef struct {
    int fd;            // File descriptor for the database file
    void* mem_start;   // Start of memory-mapped region for database file
    PageTracker* free_page_map;   // Tracks free pages and global number of frees in a Radix Tree in memory - made by reading free_pages_list[] in main DB header
    uint8_t flags;              // Read-only mode flag
} DatabasePager;

typedef struct {
    int fd;             // File descriptor for the journal file
    void* mem_start;    // Start of memory-mapped region for journal file
    PageTracker* free_page_map;   // Tracks free pages and global number of frees in a Radix Tree in memory - made by reading free_pages_list[] in main Journal header
    uint8_t flags;              // Enabled?
} JournalPager;

/* Pager */
typedef struct {
    char* filename;             // Database/Journal filename without the extension (.pseql or .pseql-journal)
    DatabasePager db_pager;
    JournalPager journal_pager;
} Pager;

/* Manage Free Pages via Radix Tree */
void init_free_page_map(Pager* pager);

// Mark a page number as free
void mark_page_free(Pager* pager, uint16_t page_no);

// Mark a page number as allocated
void mark_page_used(Pager* pager, uint16_t page_no);

// Get a free page number, or return 0 if none 
// if no free page then use the highest known page number + 1 (stored in page header)
uint16_t get_free_page(Pager* pager);

// Sync free page map with the header's free page list
PSqlStatus sync_free_page_list(Pager* pager);

/* Page Allocation & Initialization */
DBPage* allocate_page(Pager* pager, uint16_t page_no, PageType type);

/* Specialized Factory methods for each type of Pages */
Page* init_data_page(Pager* pager, uint16_t page_no);

/* Core pager functions */
Pager* pager_open(const char* filename, int flags);  // Opens a pager object - this can be handled over to a PSql object
PSqlStatus pager_close(Pager* pager);
PSqlStatus pager_sync(Pager* pager);

/* Page access functions */
Page* pager_get_page(Pager* pager, uint16_t page_no);
PSqlStatus pager_write_page(Pager* pager, Page* page);

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager);  // Init a new DB would also clear any existing journal files 
PSqlStatus pager_verify_db(Pager* pager);  // Checks magic number, size, CRC32 checksum for a valid db file

// TODO: When to vaccum and when to free
void vaccum_page();  // Vaccums page if flag COMPRESSABLE set and free_page < FREE_SPACE_VACCUM_SIZE - updates all slots but does not change their slot id

void vaccum_all_pages(Pager *pager);  // For loops over page 1 all the way to highest_page allocated in the db metadata - runs vaccum_page() on each one - this can be done at vm boot

void vaccum_data_pages(FreeSpaceTracker* tracker);  // Vaccum only on 4-bucket radix tree with tracker - this is done when the database is online

#endif

