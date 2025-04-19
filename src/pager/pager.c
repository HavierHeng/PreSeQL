#include "pager.h"

void init_free_page_map() {
    free_page_map = radix_tree_create();
}

// Mark a page number as free
void mark_page_free(uint32_t page_no) {
    radix_tree_insert(free_page_map, page_no, (void*)1);
}

// Mark a page number as allocated
void mark_page_used(uint32_t page_no) {
    radix_tree_delete(free_page_map, page_no);
}

// Get a free page number, or return -1 if none 
// if no free page then use the highest known page number + 1 (stored in page header)
int32_t get_free_page() {
    return radix_tree_pop_min(free_page_map); // Fastest available
}

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

/* preseql.h - Open/Close DB file, as a read/write database */
PSqlStatus psql_open(PSql* db);
PSqlStatus psql_close(PSql* db);


