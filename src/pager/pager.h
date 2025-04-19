/* Aka the serializer, but also handles read/writes to disk 
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
#include "page_format.h"
#include "config.h"
#include "../algorithm/radix_tree.h"  // For efficiently finding free pages
#include "../preseql.h"  // Implement PSql Open and Close methods
#include "constants.h"


/* Pager structure */
typedef struct {
    int fd;                     // File descriptor for the database file
    int journal_fd;             // File descriptor for the journal file
    void* mem_start;            // Start of memory-mapped region
    size_t file_size;           // Current file size
    DatabaseHeader* header;     // Pointer to the database header (page 0)
    RadixTree* free_page_map;   // Tracks free pages
    Page** page_cache;          // Cache of recently used pages
    int cache_size;             // Number of pages in cache
    int dirty_pages;            // Count of dirty pages
    char* filename;             // Database filename
    char* journal_filename;     // Journal filename
    uint8_t read_only;          // Read-only mode flag
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
Page* allocate_page(Pager* pager, uint16_t page_no, PageType type);

/* Specialized Factory methods for each type of Pages */
Page* init_data_page(Pager* pager, uint16_t page_no);
Page* init_btree_leaf(Pager* pager, uint16_t page_no);
Page* init_btree_root(Pager* pager, uint16_t page_no);
Page* init_overflow_page(Pager* pager, uint16_t page_no);


/* Core pager functions */
Pager* pager_open(const char* filename, int flags);
PSqlStatus pager_close(Pager* pager);
PSqlStatus pager_sync(Pager* pager);

/* Page access functions */
Page* pager_get_page(Pager* pager, uint16_t page_no);
PSqlStatus pager_write_page(Pager* pager, Page* page);
PSqlStatus pager_flush_cache(Pager* pager);

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager);
PSqlStatus pager_verify_db(Pager* pager);

/* CRC32 checksum calculation */
uint32_t calculate_crc32(const void* data, size_t length);


#endif

