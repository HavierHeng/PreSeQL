/* Aka the serializer, handles read/writes to disk for database and journal.
 *
[ Page 0 ] Header Metadata - Stores magic number, versioning and roots of various tables

[ Page 1 ] Table Catalog (B+ Tree Root Page) - track each Index Page for each table

[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table

[ Page 3 ] Foreign Key Catalog - track Foreign Key Constraints between table to table
  
[ Page 4..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ tree nodes) - Internal nodes, and leaf nodes which point to data
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit within a page
  └─ Free Pages - Pages marked as freed in database file
 * */

#ifndef PRESEQL_PAGER_H
#define PRESEQL_PAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "constants.h"
#include "types.h"
#include "status/db.h"

#define GET_PAGE_OFFSET(page_id) (PAGE_SIZE * page_id)

/* Pager flags */
#define PAGER_READONLY           0x01  // Open DB in read-only mode
#define PAGER_WRITEABLE          0x02  // Opens DB as writeable (default)
#define PAGER_OVERWRITE          0x04  // Allow overwrite mode if file already exists
#define PAGER_JOURNALING_ENABLED 0x08  // Create a journal file
#define PAGER_DIRTY              0x10  // Pages in memory have been modified
#define PAGER_SYNC_ON_WRITE      0x20  // Call msync or fsync after every page write
#define PAGER_MEMORY_DB          0x40  // Memory-only database
#define PAGER_CRASH_RECOVERY     0x80  // Pager in recovery mode after a crash

/* Core pager functions */
Pager* init_pager(const char* filename, int flags);
PSqlStatus pager_open_db(Pager* pager);
PSqlStatus pager_close_db(Pager* pager);
PSqlStatus pager_open_journal(Pager* pager);
PSqlStatus pager_close_journal(Pager* pager);
PSqlStatus free_pager(Pager* pager);

/* Page access functions */
DBPage* pager_get_page(Pager* pager, uint16_t page_no);
PSqlStatus pager_write_page(Pager* pager, DBPage* page);
PSqlStatus pager_flush_cache(Pager* pager);

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager);
PSqlStatus pager_verify_db(Pager* pager);

#endif /* PRESEQL_PAGER_H */

