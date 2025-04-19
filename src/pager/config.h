/* Store constant define values for the pager subsystem */

/* Page Sizes - Check OS setting */
#define PAGE_SIZE 4096

/* Max Size of Page Headers in Bytes (regardless of type - pad if necessary) */
#define JOURNAL_META_SIZE 12 /*  TODO: Update size based on final Journal header overhead: txn_id, page_no, etc. Can leave excess if it makes it rounded - e.g 64 bytes*/
#define MAX_DATA_BYTES (PAGE_SIZE - JOURNAL_META_SIZE) /* Limit usable bytes for data in data pages, 
                                                        since journal has some overhead itself, 
                                                        but we aim for page alignment */

#define MAX_HEADER_SIZE 255  /* Max page header size in bytes - this makes it easier to know when the page data starts and ends */

/* Base Metadata Page */
#define MAGIC_NUMBER_SIZE 8  /* Magic number is a fixed sized field */
#define MAGIC_NUMBER "SQLSHITE"  /* For magic number */
#define DB_FILE_EXTENSION ".pseql"  /* Main DB file extension */
#define JOURNAL_FILE_EXTENSION ".pseql-journal"  /* Journal file extension */
#define FREE_PAGE_LIST_BATCH_SIZE 10  // the number of changes before saving is set by a BATCH_SIZE variable

/* B+ Tree Index Page */
#define BTREE_ORDER 256 /* Max keys per B+ Tree node - i.e max number of children a node can have/fanout. 
                        Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes */

/* Data Page */
#define MAX_DATA_PER_SLOT 255  /* Max data for a slot - if exceeded it will go into an Overflow Page - this is reasonable to implement a VARCHAR(255) as TEXT (1 byte is for Null term) */

/* Overflow Page */
#define MAX_OVERFLOW_CHUNKS 256  /* Max chunks that can be stored in an Overflow page */

/* Catalog Pages */
