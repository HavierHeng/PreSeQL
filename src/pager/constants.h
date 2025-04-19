/* Store constant define values for the pager subsystem */

#ifndef PRESEQL_PAGER_CONSTANTS_H
#define PRESEQL_PAGER_CONSTANTS_H


/* Page Sizes - Check OS setting */
#define PAGE_SIZE 4096
#define MAX_PAGES 65535  /* 2^16 so all page counts are represented by uint_16 */

/* Max Size of Page Headers in Bytes (regardless of type - pad if necessary) */
#define JOURNAL_HEADER_SIZE 32  /* Size of journal header in bytes - Fixed size to make it easier to ensure enough space is reserved to align to 4096B page */
#define MAX_DATA_BYTES (PAGE_SIZE - JOURNAL_HEADER_SIZE) /* Limit usable bytes for data in data pages, 
                                                        since journal has some overhead itself, 
                                                        but we aim for page alignment */

#define MAX_HEADER_SIZE 255  /* Max page header size in bytes - this makes it easier to know when the page data starts and ends */

/* Base Metadata Page */
#define MAGIC_NUMBER_SIZE 8  /* Magic number is a fixed sized field */
#define MAGIC_NUMBER "SQLSHITE"  /* For magic number */
#define DB_FILE_EXTENSION ".pseql"  /* Main DB file extension */
#define JOURNAL_FILE_EXTENSION ".pseql-journal"  /* Journal file extension */
#define FREE_PAGE_LIST_BATCH_SIZE 10  // the number of changes before saving is set by a BATCH_SIZE variable


/* Generic Page flags */
// Page status flags
#define PAGE_DIRTY 0x01
#define PAGE_FREE  0x02

// Database flags
#define DB_READONLY 0x01
#define DB_JOURNAL_ENABLED 0x02
#define DB_CORRUPT 0x04



/* B+ Tree Index Page */
#define BTREE_ORDER 256 /* Max keys per B+ Tree node - i.e max number of children a node can have/fanout. 
                        Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes */

/* Data Page */
#define MAX_DATA_PER_SLOT 255  /* Max data for a slot - if exceeded it will go into an Overflow Page - this is reasonable to implement a VARCHAR(255) as TEXT (1 byte is for Null term) */

/* Overflow Page */
#define MAX_OVERFLOW_CHUNKS 256  /* Max chunks that can be stored in an Overflow page */

/* Catalog Pages */

#endif 
