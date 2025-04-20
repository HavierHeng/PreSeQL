#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stddef.h>
#include "pager/constants.h"
#include "journal_format.h"
#include "algorithm/crc.h"

// Static variables for journal state
static int journal_fd = -1;
static void* journal_mem = NULL;
static size_t journal_size = 0;
static JournalHeader* journal_header = NULL;
static char* journal_filename = NULL;

// Initialize the journal
PSqlStatus journal_init(const char* db_filename) {
    if (!db_filename) return PSQL_ERROR;
    
    // Create journal filename
    size_t filename_len = strlen(db_filename) + strlen(JOURNAL_FILE_EXTENSION) + 1;
    journal_filename = (char*)malloc(filename_len);
    if (!journal_filename) return PSQL_NOMEM;
    
    snprintf(journal_filename, filename_len, "%s%s", db_filename, JOURNAL_FILE_EXTENSION);
    
    // Open or create journal file
    journal_fd = open(journal_filename, O_RDWR | O_CREAT, 0644);
    if (journal_fd < 0) {
        free(journal_filename);
        journal_filename = NULL;
        return PSQL_IOERR;
    }
    
    // Get file size - to check if Journal needs to have its header recreated
    struct stat st;
    if (fstat(journal_fd, &st) < 0) {
        close(journal_fd);
        journal_fd = -1;
        free(journal_filename);
        journal_filename = NULL;
        return PSQL_IOERR;
    }
    
    journal_size = st.st_size;
    
    // Initialize or map existing file
    if (journal_size == 0) {
        // New journal - initialize with header
        journal_size = sizeof(JournalHeader);
        
        // Extend file to header size
        if (ftruncate(journal_fd, journal_size) < 0) {
            close(journal_fd);
            journal_fd = -1;
            free(journal_filename);
            journal_filename = NULL;
            return PSQL_IOERR;
        }
    }
    
    // Memory map the file
    journal_mem = mmap(NULL, journal_size, PROT_READ | PROT_WRITE, MAP_SHARED, journal_fd, 0);
    if (journal_mem == MAP_FAILED) {
        close(journal_fd);
        journal_fd = -1;
        free(journal_filename);
        journal_filename = NULL;
        return PSQL_IOERR;
    }
    
    // Point to journal header
    journal_header = (JournalHeader*)journal_mem;
    
    // Initialize header if new journal
    if (journal_size == sizeof(JournalHeader)) {
        memset(journal_header, 0, sizeof(JournalHeader));
        
        // Set magic number
        memcpy(journal_header->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE);
        
        // Set version and page size
        journal_header->version = 1;
        journal_header->page_size = PAGE_SIZE;
        journal_header->highest_txn_id = 0;
        
        // Calculate checksum
        journal_header->checksum = calculate_crc32(journal_header, 
                                                 offsetof(JournalHeader, checksum));
        
        // Sync to disk
        if (msync(journal_mem, journal_size, MS_SYNC) < 0) {
            munmap(journal_mem, journal_size);
            journal_mem = NULL;
            close(journal_fd);
            journal_fd = -1;
            free(journal_filename);
            journal_filename = NULL;
            return PSQL_IOERR;
        }
    } else {
        // Verify existing journal
        if (memcmp(journal_header->magic, MAGIC_NUMBER, MAGIC_NUMBER_SIZE) != 0) {
            munmap(journal_mem, journal_size);
            journal_mem = NULL;
            close(journal_fd);
            journal_fd = -1;
            free(journal_filename);
            journal_filename = NULL;
            return PSQL_CORRUPT;
        }
        
        // Verify checksum
        uint32_t stored_checksum = journal_header->checksum;
        journal_header->checksum = 0;
        uint32_t calculated_checksum = calculate_crc32(journal_header, 
                                                     offsetof(JournalHeader, checksum));
        journal_header->checksum = stored_checksum;
        
        if (stored_checksum != calculated_checksum) {
            munmap(journal_mem, journal_size);
            journal_mem = NULL;
            close(journal_fd);
            journal_fd = -1;
            free(journal_filename);
            journal_filename = NULL;
            return PSQL_CORRUPT;
        }
    }
    
    return PSQL_OK;
}

// Close the journal
PSqlStatus journal_close() {
    if (journal_fd < 0 || !journal_mem) return PSQL_OK; // Already closed
    
    // Sync to disk
    if (msync(journal_mem, journal_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    // Unmap memory
    if (munmap(journal_mem, journal_size) < 0) {
        return PSQL_IOERR;
    }
    
    // Close file
    close(journal_fd);
    
    // Free resources
    free(journal_filename);
    
    // Reset state
    journal_fd = -1;
    journal_mem = NULL;
    journal_size = 0;
    journal_header = NULL;
    journal_filename = NULL;
    
    return PSQL_OK;
}

// Begin a new transaction
PSqlStatus journal_begin_transaction(uint32_t txn_id) {
    if (journal_fd < 0 || !journal_mem) return PSQL_ERROR;
    
    // Check if transaction ID is valid (should be sequential)
    if (txn_id != journal_header->highest_txn_id + 1) {
        // Invalid transaction ID - clear journal and reset
        if (ftruncate(journal_fd, sizeof(JournalHeader)) < 0) {
            return PSQL_IOERR;
        }
        
        // Update size
        journal_size = sizeof(JournalHeader);
        
        // Reset header
        journal_header->highest_txn_id = txn_id - 1;
    }
    
    // Update highest transaction ID
    journal_header->highest_txn_id = txn_id;
    
    // Update checksum
    journal_header->checksum = calculate_crc32(journal_header, 
                                             offsetof(JournalHeader, checksum));
    
    // Sync to disk
    if (msync(journal_mem, sizeof(JournalHeader), MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

// Add a page to the journal
PSqlStatus journal_add_page(uint32_t txn_id, uint32_t page_no, const uint8_t* page_data, uint16_t data_size) {
    if (journal_fd < 0 || !journal_mem || !page_data) return PSQL_ERROR;
    
    // Check if transaction ID is valid
    if (txn_id != journal_header->highest_txn_id) {
        return PSQL_ERROR;
    }
    
    // Calculate new journal entry size
    size_t entry_size = sizeof(RollbackJournalPage);
    
    // Extend journal file if needed
    size_t new_size = journal_size + entry_size;
    if (ftruncate(journal_fd, new_size) < 0) {
        return PSQL_IOERR;
    }
    
    // Remap with new size
    if (munmap(journal_mem, journal_size) < 0) {
        return PSQL_IOERR;
    }
    
    journal_mem = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, journal_fd, 0);
    if (journal_mem == MAP_FAILED) {
        journal_mem = NULL;
        return PSQL_IOERR;
    }
    
    // Update pointers
    journal_header = (JournalHeader*)journal_mem;
    
    // Create journal entry
    RollbackJournalPage* entry = (RollbackJournalPage*)((uint8_t*)journal_mem + journal_size);
    entry->txn_id = txn_id;
    entry->original_page = page_no;
    entry->data_size = data_size;
    entry->reserved = 0;
    
    // Copy page data
    memcpy(entry->page_data, page_data, data_size);
    
    // Update journal size
    journal_size = new_size;
    
    // Sync to disk
    if (msync((uint8_t*)journal_mem + journal_size - entry_size, entry_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

// Commit a transaction
PSqlStatus journal_commit_transaction(uint32_t txn_id) {
    if (journal_fd < 0 || !journal_mem) return PSQL_ERROR;
    
    // Check if transaction ID is valid
    if (txn_id != journal_header->highest_txn_id) {
        return PSQL_ERROR;
    }
    
    // For commit, we just truncate the journal back to header size
    if (ftruncate(journal_fd, sizeof(JournalHeader)) < 0) {
        return PSQL_IOERR;
    }
    
    // Remap with new size
    if (munmap(journal_mem, journal_size) < 0) {
        return PSQL_IOERR;
    }
    
    journal_size = sizeof(JournalHeader);
    journal_mem = mmap(NULL, journal_size, PROT_READ | PROT_WRITE, MAP_SHARED, journal_fd, 0);
    if (journal_mem == MAP_FAILED) {
        journal_mem = NULL;
        return PSQL_IOERR;
    }
    
    // Update pointers
    journal_header = (JournalHeader*)journal_mem;
    
    // Update header
    journal_header->highest_txn_id = txn_id;
    
    // Update checksum
    journal_header->checksum = calculate_crc32(journal_header, 
                                             offsetof(JournalHeader, checksum));
    
    // Sync to disk
    if (msync(journal_mem, journal_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

// Rollback a transaction - 1 transaction at a time
// If there is multiple transaction rollbacks then call this more than once
PSqlStatus journal_rollback_transaction(uint32_t txn_id) {
    if (journal_fd < 0 || !journal_mem) return PSQL_ERROR;
    
    // Check if transaction ID is valid
    if (txn_id != journal_header->highest_txn_id) {
        return PSQL_ERROR;
    }
    
    // For rollback, we need to apply all journal entries in reverse order. Start from the freshest changes and then keep going back by 1 transaction
    // TODO: This would be implemented by the caller (i.e VM), who would read each entry and restore the original page
    
    // After rollback, truncate the journal back to header size - effectively trhowing away entries
    if (ftruncate(journal_fd, sizeof(JournalHeader)) < 0) {
        return PSQL_IOERR;
    }
    
    // Remap with new size - prevent any oopsies with SIGBUS cos we might accidentally touch truncated areas
    if (munmap(journal_mem, journal_size) < 0) {
        return PSQL_IOERR;
    }
    
    journal_size = sizeof(JournalHeader);
    journal_mem = mmap(NULL, journal_size, PROT_READ | PROT_WRITE, MAP_SHARED, journal_fd, 0);  // updated mmap
    if (journal_mem == MAP_FAILED) {
        journal_mem = NULL;
        return PSQL_IOERR;
    }
    
    // Update pointers
    journal_header = (JournalHeader*)journal_mem;
    
    // Update header
    journal_header->highest_txn_id = txn_id - 1; // Revert to previous transaction
    
    // Update checksum
    journal_header->checksum = calculate_crc32(journal_header, 
                                             offsetof(JournalHeader, checksum));
    
    // Sync to disk
    if (msync(journal_mem, journal_size, MS_SYNC) < 0) {
        return PSQL_IOERR;
    }
    
    return PSQL_OK;
}

// Check if a transaction is valid
PSqlStatus journal_is_valid_transaction(uint32_t txn_id) {
    if (journal_fd < 0 || !journal_mem) return PSQL_ERROR;
    
    // Check if transaction ID is valid (should be sequential)
    if (txn_id != journal_header->highest_txn_id + 1) {
        return PSQL_ERROR;
    }
    
    return PSQL_OK;
}
