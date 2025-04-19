#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "db/catalog/catalog.h"

/* CRC32 table for checksum calculation */
static uint32_t crc32_table[256];
static int crc32_initialized = 0;

/* Initialize CRC32 table */
static void init_crc32_table() {
    if (crc32_initialized) return;
    
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (size_t j = 0; j < 8; j++) {
            if (c & 1) {
                c = polynomial ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }
        crc32_table[i] = c;
    }
    crc32_initialized = 1;
}

/* Calculate CRC32 checksum */
uint32_t calculate_crc32(const void* data, size_t length) {
    if (!crc32_initialized) init_crc32_table();
    
    const uint8_t* buf = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

void init_free_page_map(Pager* pager) {
    pager->free_page_map = radix_tree_create();
    
    // Load free pages from header into radix tree
    for (int i = 0; i < pager->header->free_page_count; i++) {
        uint16_t page_no = pager->header->free_page_list[i];
        if (page_no > 0) {
            radix_tree_insert(pager->free_page_map, page_no, (void*)1);
        }
    }
}

// Mark a page number as free
void mark_page_free(Pager* pager, uint16_t page_no) {
    radix_tree_insert(pager->free_page_map, page_no, (void*)1);
    
    // Update header if we have space in the inline free list
    if (pager->header->free_page_count < 256) {
        pager->header->free_page_list[pager->header->free_page_count++] = page_no;
        pager->header->dirty = 1;
    }
}

// Mark a page number as allocated
void mark_page_used(Pager* pager, uint16_t page_no) {
    radix_tree_delete(pager->free_page_map, page_no);
    
    // Update highest page if necessary
    if (page_no > pager->header->highest_page) {
        pager->header->highest_page = page_no;
        pager->header->dirty = 1;
    }
    
    // Remove from inline free list if present
    for (int i = 0; i < pager->header->free_page_count; i++) {
        if (pager->header->free_page_list[i] == page_no) {
            // Shift remaining elements
            for (int j = i; j < pager->header->free_page_count - 1; j++) {
                pager->header->free_page_list[j] = pager->header->free_page_list[j + 1];
            }
            pager->header->free_page_count--;
            pager->header->dirty = 1;
            break;
        }
    }
}

// Get a free page number, or return 0 if none
uint16_t get_free_page(Pager* pager) {
    int16_t page_no = radix_tree_pop_min(pager->free_page_map);
    
    if (page_no > 0) {
        // Remove from inline free list
        for (int i = 0; i < pager->header->free_page_count; i++) {
            if (pager->header->free_page_list[i] == page_no) {
                // Shift remaining elements
                for (int j = i; j < pager->header->free_page_count - 1; j++) {
                    pager->header->free_page_list[j] = pager->header->free_page_list[j + 1];
                }
                pager->header->free_page_count--;
                pager->header->dirty = 1;
                break;
            }
        }
        return page_no;
    }
    
    // No free pages, allocate a new one
    return pager->header->highest_page + 1;
}

// Sync free page map with the header's free page list
PSqlStatus sync_free_page_list(Pager* pager) {
    // Clear current free page list
    pager->header->free_page_count = 0;
    
    // Collect up to 256 free pages from the radix tree
    struct {
        uint16_t* freelist;
        size_t* count;
        size_t max_size;
    } data = {
        pager->header->free_page_list,
        &pager->header->free_page_count,
        256
    };
    
    radix_to_freelist(pager->free_page_map, pager->header->free_page_list, 256);
    pager->header->free_page_count = data.count;
    pager->header->dirty = 1;
    
    return PSQL_OK;
}

/* Page Allocation & Initialization */
Page* allocate_page(Pager* pager, uint16_t page_no, PageType type) {
    // For memory-mapped files, get the page from the mapped region
    Page* page = (Page*)((uint8_t*)pager->mem_start + page_no * PAGE_SIZE);
    memset(page, 0, sizeof(Page));

    page->header.page_id = page_no;
    page->header.dirty = 1;
    page->header.free = 0;
    page->header.type = type;

    switch (type) {
        case DATA:
            page->header.type_specific.data.num_slots = 0;
            page->header.type_specific.data.slot_directory_offset = MAX_DATA_BYTES;
            page->header.type_specific.data.reference_count = 0;
            page->header.type_specific.data.can_compact = 0;
            break;

        case INDEX:
            page->header.type_specific.btree.btree_type = BTREE_LEAF; // default
            page->header.type_specific.btree.free_start = sizeof(PageHeader);
            page->header.type_specific.btree.free_end = MAX_DATA_BYTES;
            page->header.type_specific.btree.total_free = MAX_DATA_BYTES - sizeof(PageHeader);
            page->header.type_specific.btree.flags = 0;
            break;

        case OVERFLOW:
            page->header.type_specific.overflow.next_overflow_page = 0;
            page->header.type_specific.overflow.payload_size = 0;
            page->header.type_specific.overflow.num_chunks = 0;
            page->header.type_specific.overflow.free_offset = sizeof(PageHeader) + 
                                                             sizeof(OverflowChunkMeta) * MAX_OVERFLOW_CHUNKS;
            page->header.type_specific.overflow.free_bytes = MAX_DATA_BYTES - 
                                                            (sizeof(PageHeader) + 
                                                             sizeof(OverflowChunkMeta) * MAX_OVERFLOW_CHUNKS);
            page->header.type_specific.overflow.reference_count = 0;
            break;
    }

    mark_page_used(pager, page_no);
    return page;
}

/* Specialized Factory methods for each type of Pages */
Page* init_data_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, DATA);
}

Page* init_btree_leaf(Pager* pager, uint16_t page_no) {
    Page* page = allocate_page(pager, page_no, INDEX);
    page->header.type_specific.btree.btree_type = BTREE_LEAF;
    return page;
}

Page* init_btree_root(Pager* pager, uint16_t page_no) {
    Page* page = allocate_page(pager, page_no, INDEX);
    page->header.type_specific.btree.btree_type = BTREE_ROOT;
    return page;
}

Page* init_overflow_page(Pager* pager, uint16_t page_no) {
    return allocate_page(pager, page_no, OVERFLOW);
}


/* Core pager functions */
Pager* pager_open(const char* filename, int flags) {
    Pager* pager = (Pager*)malloc(sizeof(Pager));
    if (!pager) return NULL;
    
    memset(pager, 0, sizeof(Pager));
    
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
    
    // Set read-only flag
    pager->read_only = (flags & O_RDONLY) != 0;
    
    // Open database file
    int open_flags = flags;
    if (!pager->read_only) {
        open_flags |= O_CREAT;
    }
    
    pager->fd = open(filename, open_flags, 0644);
    if (pager->fd < 0) {
        free(pager->journal_filename);
        free(pager->filename);
        free(pager);
        return NULL;
    }
    
    // Get file size
    struct stat st;
    if (fstat(pager->fd, &st) < 0) {
        close(pager->fd);
        free(pager->journal_filename);
        free(pager->filename);
        free(pager);
        return NULL;
    }
    
    pager->file_size = st.st_size;
    
    // Initialize or map existing file
    if (pager->file_size == 0) {
        // New database - initialize with at least one page
        if (pager->read_only) {
            // Can't create a new file in read-only mode
            close(pager->fd);
            free(pager->journal_filename);
            free(pager->filename);
            free(pager);
            return NULL;
        }
        
        // Extend file to PAGE_SIZE
        if (ftruncate(pager->fd, PAGE_SIZE) < 0) {
            close(pager->fd);
            free(pager->journal_filename);
            free(pager->filename);
            free(pager);
            return NULL;
        }
        
        pager->file_size = PAGE_SIZE;
    }
    
    // Memory map the file
    int prot = pager->read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
    pager->mem_start = mmap(NULL, pager->file_size, prot, MAP_SHARED, pager->fd, 0);
    
    if (pager->mem_start == MAP_FAILED) {
        close(pager->fd);
        free(pager->journal_filename);
        free(pager->filename);
        free(pager);
        return NULL;
    }
    
    // Point to database header
    pager->header = (DatabaseHeader*)pager->mem_start;
    
    // Initialize or verify database
    if (pager->file_size == PAGE_SIZE) {
        // New database
        if (pager_init_new_db(pager) != PSQL_OK) {
            munmap(pager->mem_start, pager->file_size);
            close(pager->fd);
            free(pager->journal_filename);
            free(pager->filename);
            free(pager);
            return NULL;
        }
    } else {
        // Existing database - verify header
        if (pager_verify_db(pager) != PSQL_OK) {
            munmap(pager->mem_start, pager->file_size);
            close(pager->fd);
            free(pager->journal_filename);
            free(pager->filename);
            free(pager);
            return NULL;
        }
    }
    
    // Initialize free page map
    init_free_page_map(pager);
    
    // Initialize page cache
    pager->cache_size = 100; // Default cache size
    pager->page_cache = (Page**)calloc(pager->cache_size, sizeof(Page*));
    if (!pager->page_cache) {
        munmap(pager->mem_start, pager->file_size);
        close(pager->fd);
        free(pager->journal_filename);
        free(pager->filename);
        free(pager);
        return NULL;
    }
    
    // Open journal file if not in read-only mode
    if (!pager->read_only) {
        pager->journal_fd = open(pager->journal_filename, O_RDWR | O_CREAT, 0644);
        if (pager->journal_fd < 0) {
            free(pager->page_cache);
            munmap(pager->mem_start, pager->file_size);
            close(pager->fd);
            free(pager->journal_filename);
            free(pager->filename);
            free(pager);
            return NULL;
        }
    }
    
    return pager;
}

PSqlStatus pager_close(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    
    // Flush any dirty pages
    PSqlStatus status = pager_flush_cache(pager);
    if (status != PSQL_OK) return status;
    
    // Sync free page list to header
    status = sync_free_page_list(pager);
    if (status != PSQL_OK) return status;
    
    // Update header checksum
    pager->header->checksum = calculate_crc32(pager->header, 
                                             offsetof(DatabaseHeader, checksum));
    
    // Sync to disk
    if (msync(pager->mem_start, pager->file_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    // Unmap memory
    if (munmap(pager->mem_start, pager->file_size) < 0) {
        return PSQL_IOERR;
    }
    
    // Close files
    close(pager->fd);
    if (pager->journal_fd >= 0) {
        close(pager->journal_fd);
    }
    
    // Free resources
    radix_tree_destroy(pager->free_page_map);
    free(pager->page_cache);
    free(pager->journal_filename);
    free(pager->filename);
    free(pager);
    
    return PSQL_OK;
}

PSqlStatus pager_sync(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    
    // Update header checksum
    pager->header->checksum = calculate_crc32(pager->header, 
                                             offsetof(DatabaseHeader, checksum));
    
    // Sync to disk
    if (msync(pager->mem_start, pager->file_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

/* Page access functions */
Page* pager_get_page(Pager* pager, uint16_t page_no) {
    if (!pager || page_no >= MAX_PAGES) return NULL;
    
    // Check if page is in cache
    for (int i = 0; i < pager->cache_size; i++) {
        if (pager->page_cache[i] && pager->page_cache[i]->header.page_id == page_no) {
            return pager->page_cache[i];
        }
    }
    
    // Check if page number is beyond current file size
    if (page_no * PAGE_SIZE >= pager->file_size) {
        // Need to extend file
        if (pager->read_only) return NULL; // Can't extend in read-only mode
        
        size_t new_size = (page_no + 1) * PAGE_SIZE;
        
        // Unmap current memory
        if (munmap(pager->mem_start, pager->file_size) < 0) {
            return NULL;
        }
        
        // Extend file
        if (ftruncate(pager->fd, new_size) < 0) {
            return NULL;
        }
        
        // Remap with new size
        pager->mem_start = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, pager->fd, 0);
        if (pager->mem_start == MAP_FAILED) {
            return NULL;
        }
        
        // Update file size and header pointer
        pager->file_size = new_size;
        pager->header = (DatabaseHeader*)pager->mem_start;
        
        // Update highest page if needed
        if (page_no > pager->header->highest_page) {
            pager->header->highest_page = page_no;
            pager->header->dirty = 1;
        }
    }
    
    // Get page from memory-mapped region
    Page* page = (Page*)((uint8_t*)pager->mem_start + page_no * PAGE_SIZE);
    
    // Add to cache (replace least recently used)
    int cache_slot = -1;
    for (int i = 0; i < pager->cache_size; i++) {
        if (!pager->page_cache[i]) {
            cache_slot = i;
            break;
        }
    }
    
    if (cache_slot < 0) {
        // Cache is full, replace first non-dirty page
        for (int i = 0; i < pager->cache_size; i++) {
            if (!pager->page_cache[i]->header.dirty) {
                cache_slot = i;
                break;
            }
        }
        
        // If all pages are dirty, flush the first one
        if (cache_slot < 0) {
            pager_write_page(pager, pager->page_cache[0]);
            cache_slot = 0;
        }
    }
    
    pager->page_cache[cache_slot] = page;
    return page;
}

PSqlStatus pager_write_page(Pager* pager, Page* page) {
    if (!pager || !page) return PSQL_ERROR;
    if (pager->read_only) return PSQL_READONLY;
    
    // Mark page as dirty
    page->header.dirty = 1;
    pager->dirty_pages++;
    
    // If using journal, add page to journal before modifying
    // (This would be implemented in the journal module)
    
    return PSQL_OK;
}

PSqlStatus pager_flush_cache(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    if (pager->read_only) return PSQL_OK; // Nothing to flush in read-only mode
    
    // No need to explicitly write pages since we're using mmap
    // Just need to sync to disk
    if (pager->dirty_pages > 0) {
        if (msync(pager->mem_start, pager->file_size, MS_SYNC) < 0) {
            return PSQL_IOERR;
        }
        pager->dirty_pages = 0;
    }
    
    return PSQL_OK;
}

/* Database initialization */
PSqlStatus pager_init_new_db(Pager* pager) {
    if (!pager || pager->read_only) return PSQL_READONLY;
    
    // Initialize header
    DatabaseHeader* header = pager->header;
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
    
    // Extend file to accommodate catalog pages (4 pages total)
    if (pager->file_size < 4 * PAGE_SIZE) {
        // Unmap current memory
        if (munmap(pager->mem_start, pager->file_size) < 0) {
            return PSQL_IOERR;
        }
        
        // Extend file
        if (ftruncate(pager->fd, 4 * PAGE_SIZE) < 0) {
            return PSQL_IOERR;
        }
        
        // Remap with new size
        pager->mem_start = mmap(NULL, 4 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pager->fd, 0);
        if (pager->mem_start == MAP_FAILED) {
            return PSQL_IOERR;
        }
        
        // Update file size and header pointer
        pager->file_size = 4 * PAGE_SIZE;
        pager->header = (DatabaseHeader*)pager->mem_start;
    }
    
    // Initialize catalog pages
    Page* table_catalog = init_btree_root(pager, 1);
    Page* column_catalog = init_btree_root(pager, 2);
    Page* fk_catalog = init_btree_root(pager, 3);
    
    // Initialize catalog tables
    PSqlStatus status;
    
    status = init_table_catalog(table_catalog);
    if (status != PSQL_OK) return status;
    
    status = init_column_catalog(column_catalog);
    if (status != PSQL_OK) return status;
    
    status = init_fk_catalog(fk_catalog);
    if (status != PSQL_OK) return status;
    
    // Sync to disk
    return pager_sync(pager);
}

PSqlStatus pager_verify_db(Pager* pager) {
    if (!pager) return PSQL_ERROR;
    
    // Check magic number
    if (memcmp(pager->header->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE) != 0) {
        return PSQL_CORRUPT;
    }
    
    // Verify checksum
    uint32_t stored_checksum = pager->header->checksum;
    pager->header->checksum = 0;
    uint32_t calculated_checksum = calculate_crc32(pager->header, 
                                                  offsetof(DatabaseHeader, checksum));
    pager->header->checksum = stored_checksum;
    
    if (stored_checksum != calculated_checksum) {
        return PSQL_CORRUPT;
    }
    
    return PSQL_OK;
}

/* Implementation of psql_open and psql_close */
PSqlStatus psql_open(PSql* db) {
    if (!db || !db->filename) return PSQL_ERROR;
    
    // Open the pager
    int flags = O_RDWR;
    if (db->flags & 0x01) { // Read-only flag
        flags = O_RDONLY;
    }
    
    Pager* pager = pager_open(db->filename, flags);
    if (!pager) {
        db->error_msg = strdup("Failed to open database file");
        return PSQL_ERROR;
    }
    
    // Store pager in db handle
    db->pager = pager;
    
    return PSQL_OK;
}

PSqlStatus psql_close(PSql* db) {
    if (!db || !db->pager) return PSQL_ERROR;
    
    // Close the pager
    PSqlStatus status = pager_close((Pager*)db->pager);
    if (status != PSQL_OK) {
        db->error_msg = strdup("Failed to close database file");
        return status;
    }
    
    // Free resources
    db->pager = NULL;
    free(db->filename);
    db->filename = NULL;
    free(db->error_msg);
    db->error_msg = NULL;
    
    return PSQL_OK;
}


