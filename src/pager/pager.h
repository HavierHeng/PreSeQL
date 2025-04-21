/* Aka the serializer, handles read/writes to disk for database and journal. Core Pager structs and functions are defined here.
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
#include "db/base/page_format.h"
#include "db/free_space.h"

#define GET_PAGE_OFFSET(page_id) (PAGE_SIZE * page_id)

/* Database handle structure */
typedef struct {
    char *filename;
    int flags;
    void *pager;  /* Pointer to pager implementation - giving it access to the underlying files for DB and Journal */
    char *error_msg;  /* In case opening the database has an error */
} PSql;

/* Separating concerns for the two Pagers */
#define PAGER_READONLY           0x01  // 0000 0001 - Open DB in read-only mode, prevents write attempts
#define PAGER_WRITEABLE          0x02  // 0000 0010 - Opens DB as writeable (default). Mutually exclusive with READONLY
#define PAGER_OVERWRITE          0x04  // 0000 0100 - Allow overwrite mode (usually false) if file already exists
#define PAGER_JOURNALING_ENABLED 0x08  // 0000 1000 - Create a journal file, enabling TCL instructions like BEGIN TRANSACTION, COMMIT and ROLLBACK
#define PAGER_DIRTY              0x10  // 0001 0000 - Pages in memory have been modified and need syncing (Placeholder for now)
#define PAGER_SYNC_ON_WRITE      0x20  // 0010 0000 - Call msync or fsync after every page write (Placeholder for now)
#define PAGER_MEMORY_DB          0x40  // 0100 0000 - Memory-only database (WIP - placeholder)
#define PAGER_CRASH_RECOVERY     0x80  // 1000 0000 - Pager in recovery mode after a crash (WIP - placeholder)

// TODO: Increase the size of the Journal file - same functions as above
/* Page Allocation & Initialization */
// TODO: Since each page has a slightly different flag they need different initializations
// They also don't have to return a DBPage - I would have just created them if the page was marked as free
/* Specialized Factory methods for each type of Pages */
DBPage* init_index_internal_page(Pager* pager, uint16_t page_no);
DBPage* init_index_leaf_page(Pager* pager, uint16_t page_no);
DBPage* init_overflow_page(Pager* pager, uint16_t page_no);

/* Core pager functions */
Pager* init_pager(const char* filename, int flags);  // Create a new pager object to track open files
PSqlStatus pager_open_db(Pager* pager);  // Opens db file
PSqlStatus pager_close_db(Pager* pager);  // Closes and cleans up db files - as well as syncs any changes to disk
PSqlStatus pager_open_journal(Pager* pager);  // Opens journal file 
PSqlStatus pager_close_journal(Pager* pager);  // Closes and cleans up journal files - as well as syncs any changes to disk
void free_pager(Pager* pager);  // Clean up pager object

/* Page access functions */
DBPage* pager_get_page(Pager* pager, uint16_t page_no);
PSqlStatus pager_write_page(Pager* pager, DBPage* page);

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager);  // Init a new DB would also clear any existing journal files - creates page 0, catalog tables and the secondary tables for the catalog tables
PSqlStatus pager_verify_db(Pager* pager);  // Checks magic number, size, CRC32 checksum for a valid db file

#endif

