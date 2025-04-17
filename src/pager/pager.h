/* Aka the serializer, but also handles read/writes to disk 
* This is also where the B-Tree is handled - since its nodes are tied to pages on disk

[ Page 0 ]
  └─ Magic number, Page size, Version
  └─ Roots of special system catalogs tables (page 1-4 )

[ Page 1 ] - track each table, its names and its root page
  └─ Table catalog (B+ Tree Root page): table_id, table name, root page, schema ID, flags (hidden/system)

[ Page 2 ] - track columns for each table
  └─ Column catalog (B+ Tree Root Page): table_id, column_name, type, order

[ Page 3 ] - track FK relations
  └─ Foreign Key catalog (B+ Tree Root Page): table_id, column_name, type, order
  
[ Page 4 ] - support multiple index types, and track root pages for each table
  └─ Index catalog (B+ Tree Index Roots): table_id, index_name, root_page, type (B+ or hash)

[ Page 5..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ trees) - Internal nodes, and leaf nodes which point to data (or other nodes)
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit
  └─ Free Pages - holes in pages created after deletion of pages (a bitmap/radix tree in the Page 0 header keeps track of what pages are free for reuse - to fill in holes)
 * */

#ifndef PRESEQL_PAGER_H
#define PRESEQL_PAGER_H

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>  // For specifying mmap() flags
#include <sys/stat.h>  // Handle structs of data by stat() functions
#include "b-tree.h"
#include "radix_tree.h"  // For efficiently finding free pages
#include "status.h"

// Page size - usually 4KB (it matches system page size for efficiency)
#define PAGE_SIZE 4096

// Max keys per node - i.e max number of children a node can have - i.e fanout
// Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes
#define ORDER 256


// First Page: File header and metadata
typedef struct {
    uint32_t magic;  // Magic number to identify DB file - SQLShite
    uint32_t version;  // File format version - used for handling changes to ABI
    uint32_t page_size;  // Page size in bytes
    uint32_t root_page;  // Page number of root node
    uint32_t first_free_page;  // First free page for alloc
    uint32_t num_pages;  // Total no. of pages in DB file
    uint32_t num_tables;  // Total no. of tables in DB
    uint32_t table_directory;  // Page number of table directory
    uint8_t reserved[4064];  // Reserve space to page align the file header 
} PSqlFileHeader;

// DB context - For VM DB engine to keep track of running variables
typedef struct {
    int fd;  // File descriptor of open DB file
    void *map_addr;  // Start address of mmap
    size_t file_size;  // current file size
    PSqlFileHeader *header;  // Pointer to file header
} PSqlDBContext;


static PSqlStatus psql_open_db(const char* filename, int create_if_not_exists);  /* Opens file on disk */
static PSqlStatus psql_serialize_db(PSqlDBContext *db);  /* Converts in-memory representation of database to a format to be saved on disk */
static PSqlStatus psql_close_db();  /* Close the database file. Cleans up. Writes file onto disk via immediately syncing to disk.*/

static PSqlStatus psql_open_table();  /* Gets necessary pages for table by index */
static PSqlStatus psql_close_table();  /* Gets necessary pages for table by index */

#endif
