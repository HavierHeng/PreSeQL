/* Aka the serializer, but also handles read/writes to disk */
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
#include "btree.h"
#include "status.h"

// Page size - usually 4KB (it matches system page size for efficiency)
#define PAGE_SIZE 4096

/*
File structure of PreSeQL DB consists of:
- File header - storing metadata like magic no., version info for debug and page information such as where things are stored
- B-Tree related data - e.g internal nodes and leaf nodes
- DB VM engine context - e.g open file descriptor for DB, mmap start addresses
 */

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
