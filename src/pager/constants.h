/* Store constant define values for the pager subsystem */

#ifndef PRESEQL_PAGER_CONSTANTS_H
#define PRESEQL_PAGER_CONSTANTS_H


/* Page Sizes - Check OS setting */
#define PAGE_SIZE 4096  /* Usually either 4KB or 8KB, but more commonly 4KB */
#define MAX_PAGES 65535  /* 2^16 so all page counts are represented by uint_16 */
#define ARCH_BITS 64  /* Assumes 64-bits is the case for all new hardware and OSes - this also means this might not build on some old RPis and Microcontrollers lol git guud */
#define POINTER_SIZE (BIT_ARCH/8)  /* Assume 64-bit hardware and OS (use the right platform), this value will always be 8 bytes */


/* File names */
#define DB_FILE_EXTENSION ".pseql"  /* Main DB file extension */
#define JOURNAL_FILE_EXTENSION ".pseql-journal"  /* Journal file extension */
#define DATABASE_NAME_LENGTH 6  /* .pseql including the dot */
#define JOURNAL_NAME_LENGTH  14 /* .pseql-journal including the dot */
#define OS_MAX_FILE_NAME 255
#define MAX_FILE_NAME (OS_MAX_FILE_NAME - JOURNAL_NAME_LENGTH) /* Max Database /Journal Name (minus the largest possible extension size which is .pseql-journal)*/


/* Max Size of Page Headers in Bytes (regardless of type - strictly pad to this size in structs) */
#define MAX_PAGE_HEADER_SIZE 32  /* Max page header size in bytes - this makes it easier to know when the page data starts and ends. This affects the size of the union of headers */
#define MAX_JOURNAL_HEADER_SIZE 32  /* Max Size of journal header in bytes 
                        - Fixed size to make it easier to ensure enough space is reserved to align to 4096B page */
#define MAX_USABLE_PAGE_SIZE (PAGE_SIZE - MAX_PAGE_HEADER_SIZE - MAX_JOURNAL_HEADER_SIZE) /* Limit usable bytes for data in data pages, 
excluding the overhead from header of DB page and journal page. 
Aim for page alignment by padding. */


/* Base Metadata Page */
#define MAGIC_NUMBER_SIZE 8  /* Magic number is a fixed sized field (in bytes) */
#define MAGIC_NUMBER "SQLSHITE"  /* For magic number */
#define FREE_PAGE_LIST_SIZE 32  /* number of pages that can be cached into the free page list - in practice, since free pages will be reused up first, its rare that this number will be hit */
#define FREE_PAGE_LIST_BATCH_SIZE 10  /* the number of changes to Radix tree before flushing back to disk free page list */

/* Generic Page flags */

// Database flags
#define DB_READONLY 0x01
#define DB_JOURNAL_ENABLED 0x02
#define DB_CORRUPT 0x04

// Page type flags
#define PAGE_INDEX_INTERNAL  = 0x01  // 0000 0001 - B+ Root or Internal Node Page. Internal nodes point to other Internal nodes or Leaf nodes.
#define PAGE_INDEX_LEAF      = 0x02  // 0000 0010 - B+ Leaf Node Page - We distinguish this to separate concerns since Leaf nodes point to Data Pages.
#define PAGE_DATA            = 0x04  // 0000 0100 - Data Page
#define PAGE_OVERFLOW        = 0x08  // 0000 1000 - Overflow Page
#define PAGE_DIRTY           = 0x10  // 0001 0000 - Page has been modified since the last sync or commit. In practice, this isn't needed since `msync` is done after all modifications.
#define PAGE_FREE            = 0x20  // 0010 0000 - Page is marked as free and can be reused. In practice, we don't use this since we have the Radix tree loaded in memory.
#define PAGE_COMPACTIBLE     = 0x40  // 0100 0000 - This flag indicates whether the slots in the page is eligible for compaction. Set when changes are made to the page, but unset after VACCUM. Can hint to page begin as compacted as it can be and should be skipped over during VACCUM.
#define PAGE_PINNED          = 0x80  // 1000 0000 - Page is pinned in memory can cannot be evicted - In practice, this isn't used since `mmap` deals with paging and caching on its own via the kernel.


#define FREE_SLOT_LIST_SIZE 16  /* Logically I won't really need to exceed this value that much - if it gets reused */


/* B+ Tree Index Page */
#define MAX_DATA_PER_INDEX_SLOT 16  /* Max data for a slot used in Index Page slotted page. A slot here is fixed sized.
                        This value is lower than data page as we want a high effective order for good branching and faster search. 
                        This determines the max size of the search key for the B+ Tree.
                        All TEXT are truncated to this value if does exceed.
                        NULL will be effectively \0 for all 16 bytes.
                        INT (64-bit signed) will only use the first 8 bytes and leave the remaining 8 bytes blank. INTs are encoded to a positive range for lexicographic comparison in Index pages (though the true value in data page is still signed as twos complement).
                        - if size is exceeded in index page, it will go into an Overflow Page, and the pointer to overflow page will be set to the overflow page number and chunk.
                        */
#define INDEX_FULL_OCCUPANCY 0.8 /* Split when used space exceeds 80% of MAX_USABLE_PAGE_SIZE */
#define INDEX_MIN_OCCUPANCY 0.4 /* Rebalance when used space falls below 40% of MAX_USABLE_PAGE_SIZE */
#define INDEX_SLOT_DATA_SIZE (MAX_DATA_PER_INDEX_SLOT + 7) /* Key (16) + next_page_id (2) + next_slot_id (1) + overflow (4) */
#define SLOT_ENTRY_SIZE (sizeof(SlotEntry)) /* Typically 16 bytes: slot_id (1) + offset (8) + size (8) */

/* Data Page */
#define MAX_DATA_PER_DATA_SLOT 256  /* Max data for a slot used in both Data Page and Index Page slotted page. A slot in data page is variable in size.
                        This value is way higher than index slot, as we want to store actual row data (255 Bytes gives us up to a VARCHAR(255))
                        All TEXT are truncated to this value. 
                        NULL will be effectively \0 for all 16 bytes.
                        INT (64-bit signed) will only use the first 8 bytes and leave the remaining 8 bytes blank.
                        - if this limit is exceeded in data page, it will go into an Overflow Page, and the pointer to overflow page will be set to the overflow page number and chunk.
                        */


#define VACCUM_BATCH_SIZE 15  /* Every how many deletes to check if all pages need to be VACCUMed. VACCUM Operations are expensive but fragmentation slows things down and can make pages sparse/half-filled. */

#define FREE_SPACE_VACCUM_SIZE 255 /* Threshold for free continguous space in bytes before performing a VACCUM and rehashing to all Radix tree buckets of Index Page
*/


/* Overflow Page */


/* Catalog Pages */
#define MAX_TABLE_NAME_LENGTH 255  /* For Table catalog, Including null terminator */
#define MAX_COLUMN_NAME_LENGTH 255  /* For Column catalog, Including null terminator */


#endif 
