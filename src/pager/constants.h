/* Store constant define values for the pager subsystem */

/* Page Sizes - Check OS setting */
#define PAGE_SIZE 4096

/* Max Size of Page Headers in Bytes (regardless of type - pad if necessary) */
#define JOURNAL_META_SIZE 12 /*  TODO: Update based on Journal header: txn_id, page_no, etc. */
#define MAX_DATA_BYTES (PAGE_SIZE - JOURNAL_META_SIZE) /* Limit usable bytes for data in data pages, 
                                                        since journal has some overhead itself, 
                                                        but we aim for page alignment */

/* Base Metadata Page */
#define MAGIC_NUMBER "SQLshite"  // For magic number
#define DB_FILE_EXTENSION ".pseql"  // 

/* B+ Tree Index Page */
#define BTREE_ORDER 256 /* Max keys per B+ Tree node - i.e max number of children a node can have/fanout. 
                        Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes */

/* Data Page */

/* Overflow Page */

/* Catalog Pages */
