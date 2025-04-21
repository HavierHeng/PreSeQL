#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "pager/db/free_space.h"

typedef struct {
    int fd;            // File descriptor for the database file
    void* mem_start;   // Start of memory-mapped region for database file
    PageTracker* free_page_map;   // Tracks free pages and global number of frees in a Radix Tree in memory - made by reading free_pages_list[] in main DB header
    FreeSpaceTracker* free_data_page_slots;  // Variable size slots in Data Page
    FreeSpaceTracker* overflow_data_page_slots;  // Variable sized chunks/slots in Overflow
    // Index Pages doesn't need radix trees - searching free_slot_list[] enough
} DatabasePager;

typedef struct {
    int fd;             // File descriptor for the journal file
    void* mem_start;    // Start of memory-mapped region for journal file
    PageTracker* free_page_map;   // Tracks free pages and global number of frees in a Radix Tree in memory - made by reading free_pages_list[] in main Journal header
} JournalPager;

/* Pager */
typedef struct {
    char* filename;             // Database/Journal filename without the extension (.pseql or .pseql-journal)
    DatabasePager db_pager;
    JournalPager journal_pager;
    uint8_t flags;              // See above for flags
} Pager;

/* Increase the size of the DB file */
void* resize_mmap(int fd, void* old_map, size_t old_size, size_t new_size);  // Helper function to ftruncate, munmap then remap the newly expanded file. This is needed to be done so not SIGBUS error. Works on all POSIX/UNIX-like rather than Linux's mremap()
uint16_t allocate_new_db_page(Pager* pager);  // Add one page to the DB, returns the new page number
uint16_t allocate_new_db_pages(Pager* pager, size_t num_pages);  // Expand the db file by a few pages, useful for initializing DB file - returns the highest new page number allocated

