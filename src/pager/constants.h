/* Store constant define values for the pager subsystem */

#ifndef PRESEQL_PAGER_CONSTANTS_H
#define PRESEQL_PAGER_CONSTANTS_H


/* Page Sizes - Check OS setting */
#define PAGE_SIZE 4096  /* Usually either 4KB or 8KB, but more commonly 4KB */
#define MAX_PAGES 65535  /* 2^16 so all page counts are represented by uint_16 */
#define ARCH_BITS 64  /* Safe to assume this is the case for all new hardware and OSes - this also means this might not build on some old RPis and Microcontrollers lol git guud */
#define POINTER_SIZE (BIT_ARCH/8)  /* On 64-bit hardware and OS (yes use the right version), this value will always be 8 bytes */


/* File names */
#define DB_FILE_EXTENSION ".pseql"  /* Main DB file extension */
#define JOURNAL_FILE_EXTENSION ".pseql-journal"  /* Journal file extension */
#define DATABASE_NAME_LENGTH 6  /* .pseql including the dot */
#define JOURNAL_NAME_LENGTH  14 /* .pseql-journal including the dot */
#define OS_MAX_FILE_NAME 255
#define MAX_FILE_NAME (OS_MAX_FILE_NAME - JOURNAL_NAME_LENGTH) /* Max Database /Journal Name (minus the largest possible extension size which is .pseql-journal)*/


/* Max Size of Page Headers in Bytes (regardless of type - strictly pad to this size in structs) */
#define MAX_PAGE_HEADER_SIZE 16  /* Max page header size in bytes - this makes it easier to know when the page data starts and ends. This affects the size of the union of headers */
#define MAX_JOURNAL_HEADER_SIZE 16  /* Max Size of journal header in bytes 
                        - Fixed size to make it easier to ensure enough space is reserved to align to 4096B page */
#define MAX_USABLE_PAGE_SIZE (PAGE_SIZE - MAX_PAGE_HEADER_SIZE - MAX_JOURNAL_HEADER_SIZE) /* Limit usable bytes for data in data pages, 
excluding the overhead from header of DB page and journal page. 
Aim for page alignment by padding. */


/* Base Metadata Page */
#define MAGIC_NUMBER_SIZE 8  /* Magic number is a fixed sized field (in bytes) */
#define MAGIC_NUMBER "SQLSHITE"  /* For magic number */
#define FREE_PAGE_LIST_BATCH_SIZE 10  /* the number of changes to Radix tree before flushing back to disk free page list */
// TODO: Radix tree has to be wrapped in another struct that can count the number of changes


/* Generic Page flags */

// Database flags
#define DB_READONLY 0x01
#define DB_JOURNAL_ENABLED 0x02
#define DB_CORRUPT 0x04

#define FREE_SLOT_LIST_SIZE 16  /* Logically I won't really need to exceed this value that much - if it gets reused */


/* B+ Tree Index Page */
#define BTREE_ORDER 255 /* Max keys per B+ Tree node 
                        - i.e max number of children a node can have/fanout. 
                        In reality, this is an upper bound since the key sizes always determine the effective order of the tree 
                        - e.g if a truncated TEXT of 16 bytes is always stored with an overflow pointer of 8 bytes (64-bit system)
                        Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes */
#define MAX_DATA_PER_INDEX_SLOT 16  /* Max data for a slot used in Index Page slotted page. A slot here is fixed sized.
                        This value is lower than data page as we want a high effective order for good branching and faster search. 
                        This determines the max size of the search key for the B+ Tree.
                        All TEXT are truncated to this value if does exceed.
                        NULL will be effectively \0 for all 16 bytes.
                        INT (64-bit signed) will only use the first 8 bytes and leave the remaining 8 bytes blank. INTs are encoded to a positive range for lexicographic comparison in Index pages (though the true value in data page is still signed as twos complement).
                        - if size is exceeded in index page, it will go into an Overflow Page, and the pointer to overflow page will be set to the overflow page number and chunk.
                        */

/* Data Page */
#define MAX_DATA_PER_DATA_SLOT 256  /* Max data for a slot used in both Data Page and Index Page slotted page. A slot in data page is variable in size.
                        This value is way higher than index slot, as we want to store actual row data (255 Bytes gives us up to a VARCHAR(255))
                        All TEXT are truncated to this value. 
                        NULL will be effectively \0 for all 16 bytes.
                        INT (64-bit signed) will only use the first 8 bytes and leave the remaining 8 bytes blank.
                        - if this limit is exceeded in data page, it will go into an Overflow Page, and the pointer to overflow page will be set to the overflow page number and chunk.
                        */


#define VACCUM_BATCH_SIZE 15  /* Every how many deletes to check if all pages need to be VACCUMed. VACCUM Operations are expensive but fragmentation slows things down and can make pages sparse/half-filled. */

#define INDEX_SLOT_VACCUM_SIZE 255 /* Threshold for free continguous space before performing a VACCUM and rehashing to all Radix tree buckets of Index Page
*/


/* Overflow Page */
#define MAX_OVERFLOW_CHUNKS 255  /* Max chunks that can be stored in an Overflow page, a chunk is defined as effectively a slot of data */

/* Catalog Pages */
#define MAX_TABLE_NAME_LENGTH 255  /* For Table catalog, Including null terminator */
#define MAX_COLUMN_NAME_LENGTH 255  /* For Table catalog, Including null terminator */

#endif 
