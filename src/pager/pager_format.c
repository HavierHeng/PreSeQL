#include "pager_format.h"

void* resize_mmap(int fd, void* old_map, size_t old_size, size_t new_size);  // Helper function to ftruncate, munmap then remap the newly expanded file. This is needed to be done so not SIGBUS error. Works on all POSIX/UNIX-like rather than Linux's mremap()
uint16_t allocate_new_db_page(Pager* pager);  // Add one page to the DB, returns the new page number
uint16_t allocate_new_db_pages(Pager* pager, size_t num_pages);  // Expand the db file by a few pages, useful for initializing DB file - returns the highest new page number allocated
