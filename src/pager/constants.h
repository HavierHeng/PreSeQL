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
#define MAX_PAGE_HEADER_SIZE 127  /* Max page header size in bytes - this makes it easier to know when the page data starts and ends */
#define MAX_JOURNAL_HEADER_SIZE 31  /* Max Size of journal header in bytes 
                        - Fixed size to make it easier to ensure enough space is reserved to align to 4096B page */
#define MAX_USABLE_PAGE_BYTES (PAGE_SIZE - MAX_JOURNAL_HEADER_SIZE) /* Limit usable bytes for data in data pages, 
                                                        since journal has some overhead itself, 
                                                        but we aim for page alignment */


/* Base Metadata Page */
#define MAGIC_NUMBER_SIZE 8  /* Magic number is a fixed sized field */
#define MAGIC_NUMBER "SQLSHITE"  /* For magic number */
#define FREE_PAGE_LIST_BATCH_SIZE 10  /* the number of changes to Radix tree before flushing back to disk free page list */


/* Generic Page flags */
// Page status flags
#define PAGE_DIRTY 0x01
#define PAGE_FREE  0x02

// Database flags
#define DB_READONLY 0x01
#define DB_JOURNAL_ENABLED 0x02
#define DB_CORRUPT 0x04



/* B+ Tree Index Page */
#define BTREE_ORDER 256 /* Max keys per B+ Tree node 
                        - i.e max number of children a node can have/fanout. 
                        In reality, this is an upper bound since the key sizes always determine the effective order of the tree 
                        - e.g if a truncated TEXT of 16 bytes is always stored with an overflow pointer of 8 bytes (64-bit system)
                        Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes */

/* Data Page */
#define MAX_DATA_PER_SLOT 255  /* Max data for a slot - if exceeded it will go into an Overflow Page - this is reasonable to implement a VARCHAR(255) as TEXT (1 byte is for Null term) */

/* Overflow Page */
#define MAX_OVERFLOW_CHUNKS 256  /* Max chunks that can be stored in an Overflow page */

/* Catalog Pages */
#define MAX_TABLE_NAME_LENGTH 255  /* Including null terminator */

#endif 
