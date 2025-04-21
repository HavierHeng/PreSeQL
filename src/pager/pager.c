#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include "pager.h"
#include "pager/constants.h"
#include "pager_format.h"
#include "pager/db/free_space.h"
#include "algorithm/crc.h"
#include "status/db.h"

void* resize_mmap(int fd, void* old_map, size_t old_size, size_t new_size) {
    // Ensure file is big enough
    if (ftruncate(fd, new_size) != 0) {
        perror("ftruncate");
        return NULL;
    }

    // Unmap old region
    if (munmap(old_map, old_size) != 0) {
        perror("munmap");
        return NULL;
    }

    // Map new region
    void* new_map = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (new_map == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    return new_map;
}

// Grows the file by one page and returns a pointer to the new page - do the same for journal. Returns the page id of the page allocated
uint16_t allocate_new_db_page(Pager* pager) {
    DatabasePager* db = &pager->db_pager;

    // Determine current mapped size
    off_t current_size = lseek(db->fd, 0, SEEK_END);
    if (current_size < 0) {
        perror("lseek");
        return 0; // Return 0 instead of NULL for uint16_t return type
    }

    // Calculate new size
    size_t new_size = current_size + PAGE_SIZE;

    // Resize and remap
    void* new_map = resize_mmap(db->fd, db->mem_start, current_size, new_size);
    if (!new_map) return 0; // Return 0 instead of NULL for uint16_t return type

    db->mem_start = new_map;

    // Return the page id of the newly allocated page
    uint16_t page_id = current_size / PAGE_SIZE;
    return page_id;
}

// Probs more useful if you initializing DB file - you will always attempt to create multiple pages at one go. Returns the highest page id allocated.
uint16_t allocate_new_db_pages(Pager* pager, size_t num_pages) {
    if (num_pages == 0) return 0; // Return 0 instead of NULL for uint16_t return type

    DatabasePager* db = &pager->db_pager;

    // Determine current file size
    off_t old_size = lseek(db->fd, 0, SEEK_END);
    if (old_size < 0) {
        perror("lseek");
        return 0; // Return 0 instead of NULL for uint16_t return type
    }

    size_t growth = num_pages * PAGE_SIZE;
    size_t new_size = old_size + growth;

    // Resize and remap
    void* new_map = resize_mmap(db->fd, db->mem_start, old_size, new_size);
    if (!new_map) return 0; // Return 0 instead of NULL for uint16_t return type

    db->mem_start = new_map;

    // Return the highest page id allocated
    uint16_t start_page_id = old_size / PAGE_SIZE;
    return start_page_id + num_pages - 1;
}


void init_free_page_map(Pager* pager) {
    pager->db_pager.free_page_map = malloc(sizeof(PageTracker));
    if (!pager->db_pager.free_page_map) return;
    
    pager->db_pager.free_page_map->num_frees = 0;
    pager->db_pager.free_page_map->tree = *radix_tree_create();
    
    // Get the database header from page 0
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Load free pages from header into radix tree
    for (int i = 0; i < header->free_page_count; i++) {
        uint16_t page_no = header->free_page_list[i];
        if (page_no > 0) {
            radix_tree_insert(&pager->db_pager.free_page_map->tree, page_no);
            pager->db_pager.free_page_map->num_frees++;
        }
    }
}

// Mark a page number as free
void mark_page_free(Pager* pager, uint16_t page_no) {
    radix_tree_insert(&pager->db_pager.free_page_map->tree, page_no);
    pager->db_pager.free_page_map->num_frees++;
    
    // Get the database header from page 0
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Update header if we have space in the inline free list
    if (header->free_page_count < FREE_PAGE_LIST_SIZE) {
        header->free_page_list[header->free_page_count++] = page_no;
        // Mark the header page as needing to be written back to disk
        pager_write_page(pager, header_page);
    }
}

// Mark a page number as allocated
void mark_page_used(Pager* pager, uint16_t page_no) {
    radix_tree_delete(&pager->db_pager.free_page_map->tree, page_no);
    if (pager->db_pager.free_page_map->num_frees > 0) {
        pager->db_pager.free_page_map->num_frees--;
    }
    
    // Get the database header from page 0
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Update highest page if necessary
    if (page_no > header->highest_page) {
        header->highest_page = page_no;
        // Mark the header page as needing to be written back to disk
        pager_write_page(pager, header_page);
    }
}

// Get a free page number, or return 0 if none
uint16_t get_free_page(Pager* pager) {
    uint16_t page_no = 0;
    
    // Check if we have any free pages in our tracker
    if (pager->db_pager.free_page_map->num_frees > 0) {
        // Get the minimum page number from the radix tree
        page_no = radix_tree_pop_min(&pager->db_pager.free_page_map->tree);
        
        if (page_no > 0) {
            pager->db_pager.free_page_map->num_frees--;
            
            // Get the database header from page 0
            DBPage* header_page = pager_get_page(pager, 0);
            if (!header_page) return 0;
            
            DatabaseHeader* header = (DatabaseHeader*)header_page->data;
            
            // Remove from inline free list
            for (int i = 0; i < header->free_page_count; i++) {
                if (header->free_page_list[i] == page_no) {
                    // Shift remaining elements
                    for (int j = i; j < header->free_page_count - 1; j++) {
                        header->free_page_list[j] = header->free_page_list[j + 1];
                    }
                    header->free_page_count--;
                    // Mark the header page as needing to be written back to disk
                    pager_write_page(pager, header_page);
                    break;
                }
            }
            return page_no;
        }
    }
    
    // No free pages, allocate a new one
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return 0;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    return header->highest_page + 1;
}

// Sync free page map with the header's free page list
PSqlStatus sync_free_page_list(Pager* pager) {
    // Get the database header from page 0
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return PSQL_ERROR;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Clear current free page list
    header->free_page_count = 0;
    
    // Collect up to FREE_PAGE_LIST_SIZE free pages from the radix tree
    size_t count = 0;
    size_t max_size = FREE_PAGE_LIST_SIZE;
    
    // Use radix_to_freelist to populate the header's free page list
    count = radix_to_freelist(&pager->db_pager.free_page_map->tree, 
                             header->free_page_list, 
                             max_size);
    
    header->free_page_count = count;
    
    // Mark the header page as needing to be written back to disk
    pager_write_page(pager, header_page);
    
    return PSQL_OK;
}

/* Page Allocation & Initialization */
DBPage* allocate_page(Pager* pager, uint16_t page_no, uint8_t flag) {
    // For memory-mapped files, get the page from the mapped region
    DBPage* page = (DBPage*)((uint8_t*)pager->db_pager.mem_start + page_no * PAGE_SIZE);
    memset(page, 0, sizeof(DBPage));

    page->header.page_id = page_no;
    page->header.ref_counter = 1;
    page->header.flag = flag;
    page->header.free_start = sizeof(DBPageHeader);
    page->header.free_end = MAX_USABLE_PAGE_SIZE;
    page->header.free_total = MAX_USABLE_PAGE_SIZE - sizeof(DBPageHeader);
    page->header.total_slots = 0;
    page->header.highest_slot = 0;
    page->header.free_slot_count = 0;

    // Mark the page as used
    mark_page_used(pager, page_no);
    return page;
}

/* Specialized Factory methods for each type of Pages */
DBPage* init_data_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, PAGE_DATA);
}

DBPage* init_index_leaf_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, PAGE_INDEX_LEAF);
}

DBPage* init_index_internal_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, PAGE_INDEX_INTERNAL);
}

DBPage* init_overflow_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, PAGE_OVERFLOW);
}

/* Page access functions */
DBPage* pager_get_page(Pager* pager, uint16_t page_no) {
    if (!pager || page_no >= MAX_PAGES) return NULL;
    
    // Get page from memory-mapped region
    DBPage* page = (DBPage*)((uint8_t*)pager->db_pager.mem_start + page_no * PAGE_SIZE);
    
    return page;
}

PSqlStatus pager_write_page(Pager* pager, DBPage* page) {
    if (!pager || !page) return PSQL_ERROR;
    if (pager->flags & PAGER_READONLY) return PSQL_READONLY;
    
    // For memory-mapped files, we don't need to do anything special
    // The changes are already in the mapped memory
    
    // If journaling is enabled, we would add the page to the journal here
    if (pager->flags & PAGER_JOURNALING_ENABLED) {
        // Journal implementation would go here
    }
    
    // If sync-on-write is enabled, sync to disk immediately
    if (pager->flags & PAGER_SYNC_ON_WRITE) {
        if (msync(pager->db_pager.mem_start, pager->db_pager.file_size, MS_SYNC) < 0) {
            return PSQL_IOERR;
        }
    }
    
    return PSQL_OK;
}

// Flush all dirty pages to disk
PSqlStatus pager_flush_cache(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    if (pager->read_only) return PSQL_OK; // Nothing to flush in read-only mode
    
    // For memory-mapped files, we just need to sync the entire mapped region
    if (msync(pager->db_pager.mem_start, pager->db_pager.file_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager) {
    if (!pager || (pager->flags & PAGER_READONLY)) return PSQL_READONLY;
    
    // Allocate space for at least 4 pages (header + 3 catalog pages)
    uint16_t highest_page = allocate_new_db_pages(pager, 4);
    if (highest_page < 3) return PSQL_NOMEM;
    
    // Get the header page
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return PSQL_ERROR;
    
    // Initialize header
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    memset(header, 0, sizeof(DatabaseHeader));
    
    // Set magic number
    memcpy(header->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE);
    
    // Set basic header fields
    header->page_size = PAGE_SIZE;
    header->db_version = 1;
    header->root_table_catalog = 1;  // Page 1
    header->root_column_catalog = 2; // Page 2
    header->root_fk_catalog = 3;     // Page 3
    header->free_page_count = 0;
    header->highest_page = 3;        // First 4 pages are reserved
    header->transaction_state = 0;
    header->flags = 0;
    
    // Calculate checksum
    header->checksum = calculate_crc32(header, offsetof(DatabaseHeader, checksum));
    
    // Initialize catalog pages
    DBPage* table_catalog = init_index_internal_page(pager, 1);
    DBPage* column_catalog = init_index_internal_page(pager, 2);
    DBPage* fk_catalog = init_index_internal_page(pager, 3);
    
    if (!table_catalog || !column_catalog || !fk_catalog) {
        return PSQL_ERROR;
    }
    
    // Write the pages to disk
    pager_write_page(pager, header_page);
    pager_write_page(pager, table_catalog);
    pager_write_page(pager, column_catalog);
    pager_write_page(pager, fk_catalog);
    
    return PSQL_OK;
}


// Perform checks on the validity of a PreSeQl DB file
// These are all on the magic number (invalid file type)
// CRC32 on header (except the header checksum - we zero before recalculating)
PSqlStatus pager_verify_db(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    
    // Get the header page
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return PSQL_ERROR;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Check magic number
    if (memcmp(header->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE) != 0) {
        return PSQL_CORRUPT;
    }
    
    // Verify checksum
    uint32_t stored_checksum = header->checksum;
    header->checksum = 0;
    uint32_t calculated_checksum = calculate_crc32(header, 
                                                  offsetof(DatabaseHeader, checksum));
    header->checksum = stored_checksum;
    
    if (stored_checksum != calculated_checksum) {
        return PSQL_CORRUPT;
    }
    
    return PSQL_OK;
}

// Helper for free 
static int is_slot_free(DBPage* page, uint8_t slot_id) {
    for (uint8_t i = 0; i < page->header.free_slot_count; i++) {
        if (page->header.free_slot_list[i] == slot_id) {
            return 1;
        }
    }
    return 0;
}

// Vaccum fragmented chunks in Page
void vacuum_page(DBPage* page) {
    if (!page || page->header.page_id >= MAX_PAGES || page->header.free_total > MAX_USABLE_PAGE_SIZE) {
        return;
    }

    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    uint8_t total_slots = page->header.total_slots;
    if (total_slots == 0) {
        return;
    }

    SlotEntry active_entries[total_slots];
    uint8_t active_count = 0;
    uint64_t total_active_size = 0;

    for (uint8_t i = 0; i < total_slots; i++) {
        if (!is_slot_free(page, entries[i].slot_id)) {
            active_entries[active_count] = entries[i];
            total_active_size += entries[i].size;
            active_count++;
        }
    }

    uint64_t new_free_end = MAX_USABLE_PAGE_SIZE;
    uint64_t new_free_start = sizeof(DBPageHeader) + active_count * sizeof(SlotEntry);
    uint64_t new_free_total = MAX_USABLE_PAGE_SIZE - new_free_start - total_active_size;

    for (uint8_t i = 0; i < active_count; i++) {
        uint64_t old_offset = active_entries[i].offset;
        uint64_t size = active_entries[i].size;
        new_free_end -= size;
        memcpy(page->data + new_free_end, page->data + old_offset, size);
        active_entries[i].offset = new_free_end;
    }

    memcpy(page->data + sizeof(DBPageHeader), active_entries, active_count * sizeof(SlotEntry));

    page->header.free_start = new_free_start;
    page->header.free_end = new_free_end;
    page->header.free_total = new_free_total;
    page->header.total_slots = active_count;
    page->header.free_slot_count = 0;
    page->header.highest_slot = active_count > 0 ? active_entries[active_count - 1].slot_id : 0;
}

PSqlStatus pager_close_db(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    
    // Flush any dirty pages
    PSqlStatus status = pager_flush_cache(pager);
    if (status != PSQL_OK) return status;
    
    // Sync free page list to header
    status = sync_free_page_list(pager);
    if (status != PSQL_OK) return status;
    
    // Get the database header from page 0
    DBPage* header_page = pager_get_page(pager, 0);
    if (!header_page) return PSQL_ERROR;
    
    DatabaseHeader* header = (DatabaseHeader*)header_page->data;
    
    // Update header checksum
    header->checksum = calculate_crc32(header, offsetof(DatabaseHeader, checksum));
    
    // Sync to disk
    if (msync(pager->db_pager.mem_start, pager->db_pager.file_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    // Unmap memory
    if (munmap(pager->db_pager.mem_start, pager->db_pager.file_size) < 0) {
        return PSQL_IOERR;
    }
    
    // Close files
    close(pager->db_pager.fd);
    if (pager->journal_pager.fd >= 0) {
        close(pager->journal_pager.fd);
    }
    
    // Free resources
    radix_tree_destroy(&pager->db_pager.free_page_map->tree);
    free(pager->db_pager.free_page_map);
    free(pager->filename);
    free(pager->journal_filename);
    free(pager);
    
    return PSQL_OK;
}

/* Core pager functions */
Pager* init_pager(const char* filename, int flags) {
    Pager* pager = (Pager*)malloc(sizeof(Pager));
    if (!pager) return NULL;

    memset(pager, 0, sizeof(Pager));
    pager->flags = flags;
    pager->read_only = (flags & PAGER_READONLY) != 0;
    pager->db_pager.fd = -1;
    pager->journal_pager.fd = -1;

    // Store filename
    pager->filename = strdup(filename);
    if (!pager->filename) {
        free(pager);
        return NULL;
    }

    // Create journal filename
    size_t journal_filename_len = strlen(filename) + strlen(JOURNAL_FILE_EXTENSION) + 1;
    pager->journal_filename = (char*)malloc(journal_filename_len);
    if (!pager->journal_filename) {
        free(pager->filename);
        free(pager);
        return NULL;
    }
    snprintf(pager->journal_filename, journal_filename_len, "%s%s", filename, JOURNAL_FILE_EXTENSION);

    return pager;
}

PSqlStatus pager_open_db(Pager* pager) {
    if (!pager) return PSQL_MISUSE;
    if (pager->db_pager.fd >= 0) return PSQL_OK; // Already open
    if (pager->read_only && !access(pager->filename, F_OK)) return PSQL_READONLY; // File doesn’t exist

    // Open database file
    int open_flags = pager->read_only ? O_RDONLY : (O_RDWR | O_CREAT);
    if (pager->flags & PAGER_OVERWRITE) open_flags |= O_TRUNC;
    pager->db_pager.fd = open(pager->filename, open_flags, 0644);
    if (pager->db_pager.fd < 0) return PSQL_IOERR;

    // Get file size
    struct stat st;
    if (fstat(pager->db_pager.fd, &st) < 0) {
        close(pager->db_pager.fd);
        pager->db_pager.fd = -1;
        return PSQL_IOERR;
    }
    pager->db_pager.file_size = st.st_size;

    // Initialize or map existing file
    if (pager->db_pager.file_size == 0 && !pager->read_only) {
        // Extend file to PAGE_SIZE
        if (ftruncate(pager->db_pager.fd, PAGE_SIZE) < 0) {
            close(pager->db_pager.fd);
            pager->db_pager.fd = -1;
            return PSQL_IOERR;
        }
        pager->db_pager.file_size = PAGE_SIZE;
    }

    // Memory map the file
    int prot = pager->read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
    pager->db_pager.mem_start = mmap(NULL, pager->db_pager.file_size, prot, MAP_SHARED, pager->db_pager.fd, 0);
    if (pager->db_pager.mem_start == MAP_FAILED) {
        close(pager->db_pager.fd);
        pager->db_pager.fd = -1;
        return PSQL_IOERR;
    }

    // Initialize free page map
    init_free_page_map(pager);
    if (!pager->db_pager.free_page_map) {
        munmap(pager->db_pager.mem_start, pager->db_pager.file_size);
        close(pager->db_pager.fd);
        pager->db_pager.fd = -1;
        return PSQL_NOMEM;
    }

    // Verify or initialize database
    PSqlStatus status;
    if (pager->db_pager.file_size == PAGE_SIZE && !pager->read_only) {
        status = pager_init_new_db(pager);
    } else {
        status = pager_verify_db(pager);
    }
    if (status != PSQL_OK) {
        munmap(pager->db_pager.mem_start, pager->db_pager.file_size);
        close(pager->db_pager.fd);
        pager->db_pager.fd = -1;
        radix_tree_destroy(&pager->db_pager.free_page_map->tree);
        free(pager->db_pager.free_page_map);
        return status;
    }

    return PSQL_OK;
}

PSqlStatus pager_open_journal(Pager* pager) {
    if (!pager) return PSQL_MISUSE;
    if (pager->read_only) return PSQL_READONLY;
    if (!(pager->flags & PAGER_JOURNALING_ENABLED)) return PSQL_OK; // Journaling disabled
    if (pager->journal_pager.fd >= 0) return PSQL_OK; // Already open

    // Open journal file
    pager->journal_pager.fd = open(pager->journal_filename, O_RDWR | O_CREAT, 0644);
    if (pager->journal_pager.fd < 0) return PSQL_IOERR;

    // Initialize journal header (placeholder)
    // TODO: Add journal header initialization if needed
    return PSQL_OK;
}

PSqlStatus pager_close_journal(Pager* pager) {
    if (!pager) return PSQL_MISUSE;
    if (pager->journal_pager.fd < 0) return PSQL_OK; // No journal open

    // Sync journal to disk
    if (fsync(pager->journal_pager.fd) < 0) {
        return PSQL_IOERR;
    }

    // Close journal file
    if (close(pager->journal_pager.fd) < 0) {
        return PSQL_IOERR;
    }
    pager->journal_pager.fd = -1;

    return PSQL_OK;
}

PSqlStatus free_pager(Pager* pager) {
    if (!pager) return PSQL_NOTFOUND;

    // Close database and journal if open
    if (pager->db_pager.fd >= 0) {
        pager_close_db(pager);
    } else if (pager->journal_pager.fd >= 0) {
        pager_close_journal(pager);
    }

    // Free resources
    free(pager->filename);
    free(pager->journal_filename);
    free(pager);
    return PSQL_OK;
}

