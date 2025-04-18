/* Aka the serializer, but also handles read/writes to disk 
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
#include "page_format.h"
#include "../algorithm/radix_tree.h"  // For efficiently finding free pages
#include "../preseql.h"  // Implement PSql Open and Close methods
#include "constants.h"


/* Manage Free Pages via Radix Tree */
RadixTree* free_page_map;  // You initialize this somewhere in DB init

void init_free_page_map();

// Mark a page number as free
void mark_page_free(uint32_t page_no);

// Mark a page number as allocated
void mark_page_used(uint32_t page_no);

// Get a free page number, or return -1 if none 
// if no free page then use the highest known page number + 1 (stored in page header)
int32_t get_free_page();

/* Page Allocation & Initialization */
Page* allocate_page(uint32_t page_no, PageType type) {
    Page* page = (Page*)malloc(sizeof(Page));
    memset(page, 0, sizeof(Page));

    page->header.dirty = 1;
    page->header.free = 0;
    page->header.type = type;

    switch (type) {
        case DATA:
            page->header.type_specific.data.num_slots = 0;
            page->header.type_specific.data.slot_directory_offset = 0;
            break;

        case INDEX:
            page->header.type_specific.btree.page_id = page_no;
            page->header.type_specific.btree.btree_type = BTREE_LEAF; // default
            page->header.type_specific.btree.free_start = sizeof(PageHeader);
            page->header.type_specific.btree.free_end = MAX_DATA_BYTES; // <= 👈 IMPORTANT!
            page->header.type_specific.btree.total_free = MAX_DATA_BYTES - sizeof(PageHeader);
            page->header.type_specific.btree.flags = 0;
            break;

        case OVERFLOW:
            page->header.type_specific.overflow.next_overflow_page = 0;
            page->header.type_specific.overflow.payload_size = 0;
            break;
    }

    mark_page_used(page_no);
    return page;
}

// TODO: free page is not needed to free() due to mmap - all i have to do is mark it as such and add it to the radix tree for tracking
/*
void free_page(Page* page, uint32_t page_no) {
    mark_page_free(page_no);
    free(page);
}
*/


/* Specialized Factory methods for each type of Pages:
 * 1) Page 0 - Base Metadata Page for the whole DB file
 * 2) Index Page
 * 3) Metadata Page
 * 4) Overflow Page
 */

Page* init_data_page(uint32_t page_no) {
    return allocate_page(page_no, DATA);
}

Page* init_btree_leaf(uint32_t page_no) {
    Page* page = allocate_page(page_no, INDEX);
    page->header.type_specific.btree.btree_type = BTREE_LEAF;
    return page;
}

Page* init_btree_root(uint32_t page_no) {
    Page* page = allocate_page(page_no, INDEX);
    page->header.type_specific.btree.btree_type = BTREE_ROOT;
    return page;
}

Page* init_overflow_page(uint32_t page_no) {
    return allocate_page(page_no, OVERFLOW);
}


/* 
* TODO: page specific functions are defined within their subfolder themselves - e.g slotted page is specific to data page - so its handled within there
*/

/*
* TODO: I need a way to open_db - with choice to overwrite or keep if exists. This means I have to include the "preseql.h" file in the base path, and implrment the psql_open and psql_close methods. I also need a helper just purely to init_db().
*
* To PSql struct - add a pointer to the base location of the mmap memory via mem_start
*/
// Give some mmap memory to create 
Page* psql_init_db(void* mem_start); 


#endif

